#include <Windows.h>
#include <d3d11_1.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <imgui.h>
#include <reshade.hpp>

#if !defined(RESHADE_API_VERSION) || RESHADE_API_VERSION < 20
#error "FrameCompare requires ReShade Add-on API 20 or newer."
#endif

#include "NgxHook.hpp"

#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace reshade::api;
using clock_type = std::chrono::steady_clock;

extern "C" __declspec(dllexport) const char *NAME = "FrameCompare";
extern "C" __declspec(dllexport) const char *AUTHOR = "Night / OpenAI-assisted";
extern "C" __declspec(dllexport) const char *DESCRIPTION =
    "FrameCompare 画面对比工具 / plugin-level Before/After comparison, freeze, animated divider, labels and portable profiles.";

namespace
{
    enum class capture_strategy : int
    {
        exact_snapshot_pair = 0,
        live_pipeline = 1,
    };

    enum class capture_mode : int
    {
        automatic = 0,
        ngx_feature18 = 1,
        before_reshade_fx = 2,
    };

    enum class display_mode : int
    {
        normal_wipe = 0,
        split_screen_cr = 1,
    };

    enum class ui_language : int
    {
        chinese = 0,
        english = 1,
    };

    enum class hud_state_source : int
    {
        hotkey_toggle = 0,
        reshade_effects_state = 1,
        hotkey_hold = 2,
        hotkey_pulse = 3,
    };

    enum class snapshot_target : int
    {
        none = 0,
        before = 1,
        after = 2,
    };

    struct hotkey
    {
        uint32_t vk = 0;
        bool ctrl = false;
        bool shift = false;
        bool alt = false;
    };

    struct hud_indicator
    {
        bool enabled = true;
        std::array<char, 96> name { 'I','n','d','i','c','a','t','o','r','\0' };
        hud_state_source source = hud_state_source::hotkey_toggle;
        hotkey key {};
        std::array<char, 192> text_on { 'O','N','\0' };
        std::array<char, 192> text_off { 'O','F','F','\0' };
        bool initial_on = false;
        float x = 0.50f;
        float y = 0.12f;
        float show_seconds = 1.0f; // 0 = persistent; new items default to a short notification

        // Runtime-only state. Never serialized.
        bool runtime_initialized = false;
        bool runtime_on = false;
        clock_type::time_point changed_at = clock_type::now();
    };

    struct settings
    {
        bool enabled = true;
        ui_language language = ui_language::chinese;
        capture_strategy strategy = capture_strategy::exact_snapshot_pair;
        capture_mode capture = capture_mode::automatic;
        bool before_on_left = true;
        display_mode display = display_mode::split_screen_cr;

        float split_position = 0.50f;
        float move_step = 0.02f;
        float hold_delay = 0.25f;
        float move_speed = 0.25f;
        float auto_sweep_speed = 0.20f;
        bool auto_sweep_pingpong = false;
        bool auto_reset_from_before = true;
        int auto_sweep_direction = 1;

        bool show_border = true;
        float border_width = 0.002f;
        float border_opacity = 1.0f;
        bool screen_drag = true;
        float drag_grab_px = 14.0f;

        bool show_labels = true;
        bool label_preview = false;
        std::array<char, 256> before_text { 'O', 'F', 'F', '\0' };
        std::array<char, 256> after_text { 'O', 'N', '\0' };
        float before_x = 0.00f;
        float before_y = 0.00f;
        float after_x = 1.00f;
        float after_y = 0.00f;
        int font_size_px = 42;
        float label_opacity = 1.0f;
        float outline_px = 2.0f;
        float osd_margin_px = 18.0f;

        hotkey hk_toggle_compare { VK_F9 };
        hotkey hk_toggle_freeze { VK_F10 };
        hotkey hk_toggle_auto { VK_F11 };
        hotkey hk_left { VK_LEFT };
        hotkey hk_right { VK_RIGHT };
        // Avoid ReShade's common Home/End bindings by default.
        hotkey hk_full_before { VK_LEFT, true, false, false };
        hotkey hk_full_after { VK_RIGHT, true, false, false };
        hotkey hk_capture_before { VK_F7 };
        hotkey hk_capture_after { VK_F8 };

        std::vector<hud_indicator> hud_indicators;

        uint32_t ngx_max_age_ms = 250;
        uint32_t d3d12_color_state = static_cast<uint32_t>(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        bool allow_unsafe_cross_device_ngx = false;
        bool verbose_logging = false;
    };

    settings g_settings;
    bool g_frozen = false;
    bool g_freeze_armed = false;
    bool g_auto_sweep_active = false;
    int g_auto_direction = 1;
    int g_hotkey_capture_id = 0;
    hotkey g_hotkey_candidate {};

    HMODULE g_addon_module = nullptr;
    HMODULE g_reshade_module = nullptr;
    std::filesystem::path g_ini_path;
    bool g_settings_dirty = false;
    clock_type::time_point g_last_ini_save = clock_type::now();

    std::string g_last_status_message;
    std::string g_last_warning_message;
    std::string g_last_error_message;
    std::atomic_bool g_logged_fx_ready { false };
    std::atomic_bool g_logged_fx_missing { false };

    const char *tr(const char *zh, const char *en)
    {
        return g_settings.language == ui_language::chinese ? zh : en;
    }

    void fc_log(reshade::log::level level, std::string_view zh, std::string_view en)
    {
        const std::string_view selected = g_settings.language == ui_language::chinese ? zh : en;
        std::string line = "FrameCompare: ";
        line.append(selected.data(), selected.size());
        reshade::log::message(level, line.c_str());

        const std::string current(selected.data(), selected.size());
        if (level == reshade::log::level::warning)
            g_last_warning_message = current;
        else if (level == reshade::log::level::error)
            g_last_error_message = current;
        else
            g_last_status_message = current;
    }

    std::string localized_runtime_text(const std::string &text)
    {
        if (g_settings.language == ui_language::english || text.empty())
            return text;

        if (text == "None") return "无";
        if (text == "Immediately before ReShade FX") return "ReShade FX 执行前";
        if (text == "Before ReShade FX (Auto fallback)") return "ReShade FX 执行前（自动模式回退）";
        if (text == "NGX Feature 18 strict mode: no fresh usable snapshot") return "严格 NGX Feature 18：当前帧没有可用的新快照";
        if (text == "NGX Feature 18 D3D11 Color (pre-Evaluate)") return "NGX Feature 18 D3D11 Color（Evaluate 前）";
        if (text == "NGX Feature 18 D3D12 Color (pre-Evaluate)") return "NGX Feature 18 D3D12 Color（Evaluate 前）";
        if (text == "NGX Feature 18 cross-device D3D12 snapshot (UNSAFE opt-in; synchronization not guaranteed)") return "NGX Feature 18 跨设备 D3D12 快照（不安全选项；无法保证同步）";
        if (text == "NGX Feature 18 D3D12->D3D11 shared snapshot (UNSAFE opt-in; synchronization not guaranteed)") return "NGX Feature 18 D3D12→D3D11 共享快照（不安全选项；无法保证同步）";
        if (text == "Newest NGX Feature 18 snapshot is too old for this frame.") return "最新 NGX Feature 18 快照对当前帧来说已经过期。";
        if (text == "NGX snapshot was found but could not be copied into the comparison texture.") return "已找到 NGX 快照，但无法复制到 FrameCompare 对比纹理。";
        if (text == "No fresh Present-stage Before image was available for this ReShade effect cycle.") return "当前 ReShade 效果周期没有新的 Present 阶段 Before 画面。";
        if (text == "NGX D3D11 snapshot belongs to a different device; safe cross-device D3D11 import is unavailable.") return "NGX D3D11 快照来自其他设备，无法安全进行跨设备 D3D11 导入。";
        if (text == "NGX uses a different D3D12 device. Cross-device capture is disabled because queue synchronization cannot be guaranteed.") return "NGX 使用了另一个 D3D12 设备；由于无法保证队列同步，跨设备捕获已关闭。";
        if (text == "Unable to open NGX D3D12 shared snapshot on the ReShade D3D12 device.") return "无法在 ReShade D3D12 设备上打开 NGX D3D12 共享快照。";
        if (text == "NGX Feature 18 is on a private D3D12 device while ReShade is D3D11. Unsafe cross-API import is disabled.") return "NGX Feature 18 位于私有 D3D12 设备，而 ReShade 是 D3D11；不安全的跨 API 导入已关闭。";
        if (text == "ReShade D3D11 device does not expose ID3D11Device1 for shared NT handle import.") return "ReShade D3D11 设备没有提供共享 NT Handle 导入所需的 ID3D11Device1。";
        if (text == "Unable to open NGX D3D12 shared texture from D3D11.") return "无法从 D3D11 打开 NGX D3D12 共享纹理。";
        if (text == "NGX snapshot API does not match the ReShade runtime API.") return "NGX 快照 API 与当前 ReShade Runtime API 不匹配。";
        if (text == "Capture waits until the ReShade settings overlay is closed, then copies the next visible backbuffer without FrameCompare OSD/composite.") return "等待 ReShade 设置面板关闭后捕获下一帧可见画面；FrameCompare 自身的 OSD 与分屏合成不会被录入。";
        if (text == "Exact visible snapshot copy failed; request remains armed for the next frame.") return "精确可见画面复制失败；捕获请求保持待命，会在下一帧继续尝试。";
        if (text == "Exact snapshot pair ready. Divider motion does not re-render either side.") return "精确 Before/After 画面对已就绪；移动分割线不会重新渲染任一侧。";
        if (text == "Snapshot saved. Capture the other side to complete the exact pair.") return "当前一侧已保存；再捕获另一侧即可完成精确画面对。";
        if (text == "Before/After captures are incompatible (size or format differs). Recapture both sides without changing output resolution/format.") return "Before/After 的尺寸或像素格式不一致；请保持输出分辨率/格式不变后重新捕获两侧。";

        return text; // 未识别的底层诊断保留原文，便于排错。
    }

    std::mutex g_runtime_mutex;
    effect_runtime *g_primary_runtime = nullptr;

    std::string trim(std::string s)
    {
        const auto first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return {};
        const auto last = s.find_last_not_of(" \t\r\n");
        return s.substr(first, last - first + 1);
    }

    bool parse_bool(const std::string &s, bool fallback)
    {
        if (s == "1" || s == "true" || s == "TRUE" || s == "yes" || s == "YES" || s == "on" || s == "ON")
            return true;
        if (s == "0" || s == "false" || s == "FALSE" || s == "no" || s == "NO" || s == "off" || s == "OFF")
            return false;
        return fallback;
    }

    template <size_t N>
    void copy_cstr(std::array<char, N> &dst, const std::string &src)
    {
        const size_t n = std::min(dst.size() - 1, src.size());
        if (n != 0)
            std::memcpy(dst.data(), src.data(), n);
        dst[n] = '\0';
    }

    std::unordered_map<std::string, std::string> load_flat_ini(const std::filesystem::path &path)
    {
        std::unordered_map<std::string, std::string> out;
        std::ifstream f(path, std::ios::binary);
        if (!f)
            return out;

        std::string section;
        std::string line;
        while (std::getline(f, line))
        {
            if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF)
                line.erase(0, 3);

            line = trim(line);
            if (line.empty() || line[0] == ';' || line[0] == '#')
                continue;
            if (line.front() == '[' && line.back() == ']')
            {
                section = trim(line.substr(1, line.size() - 2));
                continue;
            }
            const auto eq = line.find('=');
            if (eq == std::string::npos)
                continue;
            out[section + "." + trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
        }
        return out;
    }

    hotkey parse_hotkey_value(const std::string &text, hotkey fallback)
    {
        if (text.empty())
            return fallback;

        std::array<int, 4> values { static_cast<int>(fallback.vk), fallback.ctrl ? 1 : 0, fallback.shift ? 1 : 0, fallback.alt ? 1 : 0 };
        std::stringstream ss(text);
        std::string part;
        size_t index = 0;
        try
        {
            while (index < values.size() && std::getline(ss, part, ','))
                values[index++] = std::stoi(trim(part), nullptr, 0);
        }
        catch (...)
        {
            return fallback;
        }

        hotkey out;
        out.vk = static_cast<uint32_t>(std::clamp(values[0], 0, 255));
        out.ctrl = values[1] != 0;
        out.shift = values[2] != 0;
        out.alt = values[3] != 0;
        return out;
    }

    std::string hotkey_to_ini(const hotkey &key)
    {
        std::ostringstream out;
        out << key.vk << ',' << (key.ctrl ? 1 : 0) << ',' << (key.shift ? 1 : 0) << ',' << (key.alt ? 1 : 0);
        return out.str();
    }

    void load_settings()
    {
        const auto ini = load_flat_ini(g_ini_path);
        const auto get = [&](const std::string &key) -> std::string {
            const auto it = ini.find(key);
            return it == ini.end() ? std::string() : it->second;
        };
        const auto geti = [&](const std::string &key, int fallback) {
            try { const auto v = get(key); return v.empty() ? fallback : std::stoi(v, nullptr, 0); }
            catch (...) { return fallback; }
        };
        const auto getu = [&](const std::string &key, uint32_t fallback) {
            try { const auto v = get(key); return v.empty() ? fallback : static_cast<uint32_t>(std::stoul(v, nullptr, 0)); }
            catch (...) { return fallback; }
        };
        const auto getf = [&](const std::string &key, float fallback) {
            try { const auto v = get(key); return v.empty() ? fallback : std::stof(v); }
            catch (...) { return fallback; }
        };
        const auto getb = [&](const std::string &key, bool fallback) {
            const auto v = get(key);
            return v.empty() ? fallback : parse_bool(v, fallback);
        };
        const auto get_hotkey = [&](const std::string &key, hotkey fallback) {
            return parse_hotkey_value(get(key), fallback);
        };

        g_settings.enabled = getb("General.Enabled", g_settings.enabled);
        g_settings.language = static_cast<ui_language>(std::clamp(geti("General.Language", static_cast<int>(g_settings.language)), 0, 1));
        const bool legacy_v12_ini = get("General.Strategy").empty();
        g_settings.strategy = static_cast<capture_strategy>(std::clamp(geti("General.Strategy", static_cast<int>(g_settings.strategy)), 0, 1));
        int live_capture = geti("General.LiveCaptureMode", geti("General.CaptureMode", static_cast<int>(g_settings.capture)));
        if (live_capture < 0 || live_capture > 2)
            live_capture = 0;
        g_settings.capture = static_cast<capture_mode>(live_capture);
        g_settings.before_on_left = getb("General.BeforeOnLeft", g_settings.before_on_left);
        g_settings.display = static_cast<display_mode>(std::clamp(geti("General.DisplayMode", static_cast<int>(g_settings.display)), 0, 1));
        if (legacy_v12_ini)
        {
            g_settings.strategy = capture_strategy::exact_snapshot_pair;
            g_settings.display = display_mode::split_screen_cr;
        }

        g_settings.split_position = std::clamp(getf("Divider.Position", g_settings.split_position), 0.0f, 1.0f);
        g_settings.move_step = std::clamp(getf("Divider.Step", g_settings.move_step), 0.001f, 1.0f);
        g_settings.hold_delay = std::clamp(getf("Divider.HoldDelay", g_settings.hold_delay), 0.0f, 2.0f);
        g_settings.move_speed = std::clamp(getf("Divider.MoveSpeed", g_settings.move_speed), 0.001f, 4.0f);
        g_settings.auto_sweep_speed = std::clamp(getf("Divider.AutoSweepSpeed", g_settings.auto_sweep_speed), 0.001f, 4.0f);
        g_settings.auto_sweep_pingpong = getb("Divider.AutoSweepPingPong", g_settings.auto_sweep_pingpong);
        g_settings.auto_reset_from_before = getb("Divider.AutoResetFromBefore", g_settings.auto_reset_from_before);
        g_settings.auto_sweep_direction = geti("Divider.AutoSweepDirection", g_settings.auto_sweep_direction) < 0 ? -1 : 1;
        g_settings.show_border = getb("Divider.ShowBorder", g_settings.show_border);
        g_settings.border_width = std::clamp(getf("Divider.BorderWidth", g_settings.border_width), 0.0f, 0.05f);
        g_settings.border_opacity = std::clamp(getf("Divider.BorderOpacity", g_settings.border_opacity), 0.0f, 1.0f);
        g_settings.screen_drag = getb("Divider.ScreenDrag", g_settings.screen_drag);
        g_settings.drag_grab_px = std::clamp(getf("Divider.DragGrabPx", g_settings.drag_grab_px), 2.0f, 100.0f);

        g_settings.show_labels = getb("Labels.Show", g_settings.show_labels);
        g_settings.label_preview = getb("Labels.Preview", g_settings.label_preview);
        if (const auto v = get("Labels.BeforeText"); !v.empty()) copy_cstr(g_settings.before_text, v);
        if (const auto v = get("Labels.AfterText"); !v.empty()) copy_cstr(g_settings.after_text, v);
        g_settings.before_x = std::clamp(getf("Labels.BeforeX", g_settings.before_x), 0.0f, 1.0f);
        g_settings.before_y = std::clamp(getf("Labels.BeforeY", g_settings.before_y), 0.0f, 1.0f);
        g_settings.after_x = std::clamp(getf("Labels.AfterX", g_settings.after_x), 0.0f, 1.0f);
        g_settings.after_y = std::clamp(getf("Labels.AfterY", g_settings.after_y), 0.0f, 1.0f);
        g_settings.font_size_px = std::clamp(geti("Labels.FontSizePx", g_settings.font_size_px), 8, 256);
        g_settings.label_opacity = std::clamp(getf("Labels.Opacity", g_settings.label_opacity), 0.0f, 1.0f);
        g_settings.outline_px = std::clamp(getf("Labels.OutlinePx", g_settings.outline_px), 0.0f, 12.0f);
        g_settings.osd_margin_px = std::clamp(getf("Labels.MarginPx", g_settings.osd_margin_px), 0.0f, 200.0f);

        g_settings.hk_toggle_compare = get_hotkey("Hotkeys.ToggleCompare", g_settings.hk_toggle_compare);
        g_settings.hk_toggle_freeze = get_hotkey("Hotkeys.ToggleFreeze", g_settings.hk_toggle_freeze);
        g_settings.hk_toggle_auto = get_hotkey("Hotkeys.ToggleAutoSweep", g_settings.hk_toggle_auto);
        g_settings.hk_left = get_hotkey("Hotkeys.MoveLeft", g_settings.hk_left);
        g_settings.hk_right = get_hotkey("Hotkeys.MoveRight", g_settings.hk_right);
        g_settings.hk_full_before = get_hotkey("Hotkeys.FullBefore", g_settings.hk_full_before);
        g_settings.hk_full_after = get_hotkey("Hotkeys.FullAfter", g_settings.hk_full_after);
        g_settings.hk_capture_before = get_hotkey("Hotkeys.CaptureBefore", g_settings.hk_capture_before);
        g_settings.hk_capture_after = get_hotkey("Hotkeys.CaptureAfter", g_settings.hk_capture_after);

        g_settings.hud_indicators.clear();
        const int indicator_count = std::clamp(geti("HUD.Count", 0), 0, 32);
        g_settings.hud_indicators.reserve(static_cast<size_t>(indicator_count));
        for (int i = 0; i < indicator_count; ++i)
        {
            hud_indicator item;
            const std::string prefix = "HUD." + std::to_string(i) + ".";
            item.enabled = getb(prefix + "Enabled", item.enabled);
            if (const auto v = get(prefix + "Name"); !v.empty()) copy_cstr(item.name, v);
            item.source = static_cast<hud_state_source>(std::clamp(geti(prefix + "Source", static_cast<int>(item.source)), 0, 3));
            item.key = get_hotkey(prefix + "Hotkey", item.key);
            if (const auto v = get(prefix + "TextOn"); !v.empty()) copy_cstr(item.text_on, v);
            if (const auto v = get(prefix + "TextOff"); !v.empty()) copy_cstr(item.text_off, v);
            item.initial_on = getb(prefix + "InitialOn", item.initial_on);
            item.x = std::clamp(getf(prefix + "X", item.x), 0.0f, 1.0f);
            item.y = std::clamp(getf(prefix + "Y", item.y), 0.0f, 1.0f);
            item.show_seconds = std::clamp(getf(prefix + "ShowSeconds", item.show_seconds), 0.0f, 60.0f);
            item.runtime_initialized = false;
            item.runtime_on = item.initial_on;
            item.changed_at = clock_type::now();
            g_settings.hud_indicators.push_back(item);
        }

        g_settings.ngx_max_age_ms = std::clamp(getu("NGX.MaxSnapshotAgeMs", g_settings.ngx_max_age_ms), 1u, 5000u);
        g_settings.d3d12_color_state = getu("NGX.D3D12ColorState", g_settings.d3d12_color_state);
        g_settings.allow_unsafe_cross_device_ngx = getb("NGX.AllowUnsafeCrossDevice", g_settings.allow_unsafe_cross_device_ngx);
        g_settings.verbose_logging = getb("Diagnostics.VerboseLogging", g_settings.verbose_logging);

        g_frozen = false;
        g_freeze_armed = false;
        g_auto_sweep_active = false;
        g_auto_direction = g_settings.auto_sweep_direction;
        g_hotkey_capture_id = 0;
        g_hotkey_candidate = {};
    }

    void save_settings()
    {
        std::ofstream f(g_ini_path, std::ios::binary | std::ios::trunc);
        if (!f)
        {
            fc_log(reshade::log::level::warning,
                "无法写入 FrameCompare.ini。",
                "Unable to write FrameCompare.ini.");
            return;
        }

        f << "; FrameCompare v1.3 portable configuration (UTF-8)\n";
        f << "; Hotkeys are stored as VK,Ctrl,Shift,Alt. Use the in-game key capture UI instead of editing numbers.\n\n";
        f << "[General]\n";
        f << "Enabled=" << (g_settings.enabled ? 1 : 0) << "\n";
        f << "Language=" << static_cast<int>(g_settings.language) << "\n";
        f << "Strategy=" << static_cast<int>(g_settings.strategy) << "\n";
        f << "LiveCaptureMode=" << static_cast<int>(g_settings.capture) << "\n";
        f << "BeforeOnLeft=" << (g_settings.before_on_left ? 1 : 0) << "\n";
        f << "DisplayMode=" << static_cast<int>(g_settings.display) << "\n\n";

        f << "[Divider]\n";
        f << "Position=" << g_settings.split_position << "\n";
        f << "Step=" << g_settings.move_step << "\n";
        f << "HoldDelay=" << g_settings.hold_delay << "\n";
        f << "MoveSpeed=" << g_settings.move_speed << "\n";
        f << "AutoSweepSpeed=" << g_settings.auto_sweep_speed << "\n";
        f << "AutoSweepPingPong=" << (g_settings.auto_sweep_pingpong ? 1 : 0) << "\n";
        f << "AutoResetFromBefore=" << (g_settings.auto_reset_from_before ? 1 : 0) << "\n";
        f << "AutoSweepDirection=" << g_settings.auto_sweep_direction << "\n";
        f << "ShowBorder=" << (g_settings.show_border ? 1 : 0) << "\n";
        f << "BorderWidth=" << g_settings.border_width << "\n";
        f << "BorderOpacity=" << g_settings.border_opacity << "\n";
        f << "ScreenDrag=" << (g_settings.screen_drag ? 1 : 0) << "\n";
        f << "DragGrabPx=" << g_settings.drag_grab_px << "\n\n";

        f << "[Labels]\n";
        f << "Show=" << (g_settings.show_labels ? 1 : 0) << "\n";
        f << "Preview=" << (g_settings.label_preview ? 1 : 0) << "\n";
        f << "BeforeText=" << g_settings.before_text.data() << "\n";
        f << "AfterText=" << g_settings.after_text.data() << "\n";
        f << "BeforeX=" << g_settings.before_x << "\n";
        f << "BeforeY=" << g_settings.before_y << "\n";
        f << "AfterX=" << g_settings.after_x << "\n";
        f << "AfterY=" << g_settings.after_y << "\n";
        f << "FontSizePx=" << g_settings.font_size_px << "\n";
        f << "Opacity=" << g_settings.label_opacity << "\n";
        f << "OutlinePx=" << g_settings.outline_px << "\n";
        f << "MarginPx=" << g_settings.osd_margin_px << "\n\n";

        f << "[Hotkeys]\n";
        f << "ToggleCompare=" << hotkey_to_ini(g_settings.hk_toggle_compare) << "\n";
        f << "ToggleFreeze=" << hotkey_to_ini(g_settings.hk_toggle_freeze) << "\n";
        f << "ToggleAutoSweep=" << hotkey_to_ini(g_settings.hk_toggle_auto) << "\n";
        f << "MoveLeft=" << hotkey_to_ini(g_settings.hk_left) << "\n";
        f << "MoveRight=" << hotkey_to_ini(g_settings.hk_right) << "\n";
        f << "FullBefore=" << hotkey_to_ini(g_settings.hk_full_before) << "\n";
        f << "FullAfter=" << hotkey_to_ini(g_settings.hk_full_after) << "\n";
        f << "CaptureBefore=" << hotkey_to_ini(g_settings.hk_capture_before) << "\n";
        f << "CaptureAfter=" << hotkey_to_ini(g_settings.hk_capture_after) << "\n\n";

        f << "[HUD]\n";
        f << "Count=" << g_settings.hud_indicators.size() << "\n\n";
        for (size_t i = 0; i < g_settings.hud_indicators.size(); ++i)
        {
            const auto &item = g_settings.hud_indicators[i];
            f << "[HUD." << i << "]\n";
            f << "Enabled=" << (item.enabled ? 1 : 0) << "\n";
            f << "Name=" << item.name.data() << "\n";
            f << "Source=" << static_cast<int>(item.source) << "\n";
            f << "Hotkey=" << hotkey_to_ini(item.key) << "\n";
            f << "TextOn=" << item.text_on.data() << "\n";
            f << "TextOff=" << item.text_off.data() << "\n";
            f << "InitialOn=" << (item.initial_on ? 1 : 0) << "\n";
            f << "X=" << item.x << "\n";
            f << "Y=" << item.y << "\n";
            f << "ShowSeconds=" << item.show_seconds << "\n\n";
        }

        f << "[NGX]\n";
        f << "MaxSnapshotAgeMs=" << g_settings.ngx_max_age_ms << "\n";
        f << "D3D12ColorState=0x" << std::hex << std::uppercase << g_settings.d3d12_color_state << std::dec << "\n";
        f << "AllowUnsafeCrossDevice=" << (g_settings.allow_unsafe_cross_device_ngx ? 1 : 0) << "\n\n";
        f << "[Diagnostics]\n";
        f << "VerboseLogging=" << (g_settings.verbose_logging ? 1 : 0) << "\n";
        f.flush();
        g_settings_dirty = false;
        g_last_ini_save = clock_type::now();
    }

    void mark_settings_dirty()
    {
        g_settings_dirty = true;
    }

    void maybe_save_settings()
    {
        if (!g_settings_dirty)
            return;
        if (std::chrono::duration<float>(clock_type::now() - g_last_ini_save).count() >= 0.75f)
            save_settings();
    }

    float full_before_position()
    {
        return g_settings.before_on_left ? 1.0f : 0.0f;
    }

    float full_after_position()
    {
        return g_settings.before_on_left ? 0.0f : 1.0f;
    }

    int direction_toward_after()
    {
        return full_after_position() > full_before_position() ? 1 : -1;
    }

    void sync_ngx_capture_enabled()
    {
        const bool wants_ngx = g_settings.strategy == capture_strategy::live_pipeline &&
            (g_settings.capture == capture_mode::automatic || g_settings.capture == capture_mode::ngx_feature18);
        framecompare::ngx::set_d3d12_color_state(static_cast<D3D12_RESOURCE_STATES>(g_settings.d3d12_color_state));
        framecompare::ngx::set_capture_enabled(g_settings.enabled && !g_frozen && wants_ngx);
    }

    struct gpu_texture
    {
        resource tex = {};
        resource_view srv = {};
        uint32_t width = 0;
        uint32_t height = 0;
        format fmt = format::unknown;
        bool shader_state = false;
        bool ready = false;
    };

    struct parameter_texture
    {
        resource tex = {};
        resource_view srv = {};
        bool shader_state = false;
        bool available = false;
    };

    struct move_key_state
    {
        bool was_down = false;
        bool long_mode = false;
        clock_type::time_point pressed_at = clock_type::now();
    };

    struct __declspec(uuid("89E95BE8-0C08-43D9-87D9-36123E5A9A81")) runtime_state
    {
        gpu_texture before;
        gpu_texture after;
        parameter_texture params;

        effect_technique composite = {};
        bool warned_missing_fx = false;
        bool warned_param_upload = false;
        bool pair_valid = false;
        bool exact_before_valid = false;
        bool exact_after_valid = false;
        snapshot_target pending_snapshot = snapshot_target::none;
        bool fallback_before_this_cycle = false;
        bool before_this_cycle = false;
        bool overlay_open = false;
        bool divider_dragging = false;

        uint64_t effect_cycle = 0;
        uint64_t last_ngx_generation = 0;
        clock_type::time_point last_tick = clock_type::now();

        move_key_state left_key;
        move_key_state right_key;

        std::string capture_source = "None";
        std::string capture_note;
        std::string last_logged_capture_source;
        std::string last_logged_ngx_error;
        uint64_t last_logged_ngx_failures = 0;
        bool logged_pair_ready = false;
        bool warned_incompatible_pair = false;
        uint32_t before_width = 0;
        uint32_t before_height = 0;
        uint32_t after_width = 0;
        uint32_t after_height = 0;
        double latest_ngx_age_ms = -1.0;

        std::shared_ptr<const framecompare::ngx::snapshot> active_snapshot;
        ComPtr<ID3D12Resource> imported12;
        ComPtr<ID3D11Texture2D> imported11;
        const framecompare::ngx::native_capture *imported_identity = nullptr;
    };

    void destroy_gpu_texture(device *dev, gpu_texture &t)
    {
        if (t.srv != 0)
            dev->destroy_resource_view(t.srv);
        if (t.tex != 0)
            dev->destroy_resource(t.tex);
        t = {};
    }

    void destroy_parameter_texture(device *dev, parameter_texture &t)
    {
        if (t.srv != 0)
            dev->destroy_resource_view(t.srv);
        if (t.tex != 0)
            dev->destroy_resource(t.tex);
        t = {};
    }

    bool compatible_capture_desc(const gpu_texture &t, const resource_desc &src_desc)
    {
        return t.tex != 0 && t.width == src_desc.texture.width && t.height == src_desc.texture.height && t.fmt == src_desc.texture.format;
    }

    bool capture_pair_compatible(const gpu_texture &before, const gpu_texture &after)
    {
        if (!before.ready || !after.ready || before.tex == 0 || after.tex == 0)
            return false;
        if (before.width != after.width || before.height != after.height)
            return false;

        const format before_typed = format_to_default_typed(before.fmt);
        const format after_typed = format_to_default_typed(after.fmt);
        return before_typed != format::unknown && before_typed == after_typed;
    }

    void update_pair_validity(runtime_state *state, bool require_exact_snapshots)
    {
        if (state == nullptr)
            return;

        const bool captures_present = require_exact_snapshots
            ? (state->exact_before_valid && state->exact_after_valid && state->before.ready && state->after.ready)
            : (state->before.ready && state->after.ready);

        if (!captures_present)
        {
            state->pair_valid = false;
            state->warned_incompatible_pair = false;
            return;
        }

        state->pair_valid = capture_pair_compatible(state->before, state->after);
        if (state->pair_valid)
        {
            state->warned_incompatible_pair = false;
            return;
        }

        state->capture_note = "Before/After captures are incompatible (size or format differs). Recapture both sides without changing output resolution/format.";
        if (!state->warned_incompatible_pair)
        {
            fc_log(reshade::log::level::warning,
                "Before/After 尺寸或像素格式不一致，已拒绝合成。请保持输出分辨率/格式不变后重新捕获两侧。",
                "Before/After size or pixel format differs; composition is disabled. Recapture both sides without changing output resolution/format.");
            state->warned_incompatible_pair = true;
        }
    }

    bool ensure_capture_texture(effect_runtime *runtime, gpu_texture &target, resource source, const char *debug_name)
    {
        device *dev = runtime->get_device();
        const resource_desc src_desc = dev->get_resource_desc(source);
        if ((src_desc.type != resource_type::texture_2d && src_desc.type != resource_type::surface) || src_desc.texture.samples != 1)
            return false;

        if (compatible_capture_desc(target, src_desc))
            return true;

        if (target.tex != 0 || target.srv != 0)
        {
            runtime->get_command_queue()->wait_idle();
            destroy_gpu_texture(dev, target);
        }

        const format typed = format_to_default_typed(src_desc.texture.format);
        if (typed == format::unknown || !dev->check_format_support(typed, resource_usage::shader_resource))
            return false;

        resource_desc desc(
            src_desc.texture.width,
            src_desc.texture.height,
            1, 1,
            src_desc.texture.format,
            1,
            memory_heap::default_,
            resource_usage::copy_dest | resource_usage::shader_resource);

        if (!dev->create_resource(desc, nullptr, resource_usage::copy_dest, &target.tex))
            return false;
        if (!dev->create_resource_view(target.tex, resource_usage::shader_resource, resource_view_desc(typed), &target.srv))
        {
            dev->destroy_resource(target.tex);
            target = {};
            return false;
        }

        dev->set_resource_name(target.tex, debug_name);
        target.width = src_desc.texture.width;
        target.height = src_desc.texture.height;
        target.fmt = src_desc.texture.format;
        target.shader_state = false;
        target.ready = false;
        return true;
    }

    bool copy_into_capture(effect_runtime *runtime, command_list *cmd, resource source, resource_usage source_state,
        gpu_texture &dest, const char *debug_name)
    {
        if (source == 0 || cmd == nullptr)
            return false;
        if (!ensure_capture_texture(runtime, dest, source, debug_name))
            return false;

        if (dest.shader_state)
            cmd->barrier(dest.tex, resource_usage::shader_resource, resource_usage::copy_dest);
        if (source_state != resource_usage::copy_source)
            cmd->barrier(source, source_state, resource_usage::copy_source);

        cmd->copy_resource(source, dest.tex);

        if (source_state != resource_usage::copy_source)
            cmd->barrier(source, resource_usage::copy_source, source_state);
        cmd->barrier(dest.tex, resource_usage::copy_dest, resource_usage::shader_resource);
        dest.shader_state = true;
        dest.ready = true;
        return true;
    }

    bool ensure_parameter_texture(effect_runtime *runtime, runtime_state *state)
    {
        if (state->params.available)
            return true;

        device *dev = runtime->get_device();
        if (!dev->check_capability(device_caps::update_texture_region_command))
            return false;

        resource_desc desc(8, 1, 1, 1, format::r32g32b32a32_float, 1, memory_heap::default_,
            resource_usage::copy_dest | resource_usage::shader_resource);
        if (!dev->create_resource(desc, nullptr, resource_usage::copy_dest, &state->params.tex))
            return false;
        if (!dev->create_resource_view(state->params.tex, resource_usage::shader_resource,
                resource_view_desc(format::r32g32b32a32_float), &state->params.srv))
        {
            dev->destroy_resource(state->params.tex);
            state->params = {};
            return false;
        }
        dev->set_resource_name(state->params.tex, "FrameCompare Parameters");
        state->params.shader_state = false;
        state->params.available = true;
        runtime->update_texture_bindings("FRAMECOMPARE_PARAMS", state->params.srv, state->params.srv);
        return true;
    }

    bool update_parameter_texture(effect_runtime *runtime, runtime_state *state, command_list *cmd)
    {
        if (!ensure_parameter_texture(runtime, state) || cmd == nullptr)
            return false;

        std::array<float, 32> p = {};
        // texel 0: split, divider width, divider opacity, show divider
        p[0] = std::clamp(g_settings.split_position, 0.0f, 1.0f);
        p[1] = std::max(0.0f, g_settings.border_width);
        p[2] = std::clamp(g_settings.border_opacity, 0.0f, 1.0f);
        p[3] = g_settings.show_border ? 1.0f : 0.0f;
        // texel 1: before-on-left, display mode, pair-valid, reserved
        p[4] = g_settings.before_on_left ? 1.0f : 0.0f;
        p[5] = static_cast<float>(static_cast<int>(g_settings.display));
        p[6] = (g_settings.enabled && state->pair_valid && state->before.ready && state->after.ready) ? 1.0f : 0.0f;

        if (state->params.shader_state)
            cmd->barrier(state->params.tex, resource_usage::shader_resource, resource_usage::copy_dest);

        subresource_data data = {};
        data.data = p.data();
        data.row_pitch = sizeof(float) * 32;
        data.slice_pitch = data.row_pitch;
        cmd->update_texture_region(data, state->params.tex, 0);
        cmd->barrier(state->params.tex, resource_usage::copy_dest, resource_usage::shader_resource);
        state->params.shader_state = true;
        return true;
    }

    void refresh_effect_handles(effect_runtime *runtime, runtime_state *state)
    {
        const bool was_ready = state->composite != 0;
        state->composite = runtime->find_technique("FrameCompare.fx", "FrameCompareComposite");
        if (state->composite != 0)
        {
            runtime->set_technique_state(state->composite, false);
            if (!was_ready && !g_logged_fx_ready.exchange(true))
                fc_log(reshade::log::level::info,
                    "已找到 FrameCompare.fx / FrameCompareComposite，合成着色器已就绪。",
                    "FrameCompare.fx / FrameCompareComposite found; compositor is ready.");
            g_logged_fx_missing.store(false);
            state->warned_missing_fx = false;
        }
        if (state->params.srv != 0)
            runtime->update_texture_bindings("FRAMECOMPARE_PARAMS", state->params.srv, state->params.srv);
        if (state->before.srv != 0)
            runtime->update_texture_bindings("FRAMECOMPARE_BEFORE", state->before.srv, state->before.srv);
        if (state->after.srv != 0)
            runtime->update_texture_bindings("FRAMECOMPARE_AFTER", state->after.srv, state->after.srv);
    }

    bool is_primary_runtime(effect_runtime *runtime)
    {
        std::lock_guard lock(g_runtime_mutex);
        if (g_primary_runtime == nullptr)
            g_primary_runtime = runtime;
        return g_primary_runtime == runtime;
    }

    bool hotkey_modifiers_match(effect_runtime *runtime, const hotkey &key)
    {
        const bool ctrl = runtime->is_key_down(VK_CONTROL);
        const bool shift = runtime->is_key_down(VK_SHIFT);
        const bool alt = runtime->is_key_down(VK_MENU);
        return ctrl == key.ctrl && shift == key.shift && alt == key.alt;
    }

    bool hotkey_pressed(effect_runtime *runtime, const hotkey &key)
    {
        return key.vk != 0 && runtime->is_key_pressed(key.vk) && hotkey_modifiers_match(runtime, key);
    }

    bool hotkey_down(effect_runtime *runtime, const hotkey &key)
    {
        return key.vk != 0 && runtime->is_key_down(key.vk) && hotkey_modifiers_match(runtime, key);
    }

    const char *key_name(uint32_t vk)
    {
        static char text[32];
        if (vk == 0) return tr("未设置", "Not set");
        if (vk >= VK_F1 && vk <= VK_F24)
        {
            std::snprintf(text, sizeof(text), "F%u", vk - VK_F1 + 1);
            return text;
        }
        if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9'))
        {
            text[0] = static_cast<char>(vk);
            text[1] = '\0';
            return text;
        }
        switch (vk)
        {
        case VK_LEFT: return "Left";
        case VK_RIGHT: return "Right";
        case VK_UP: return "Up";
        case VK_DOWN: return "Down";
        case VK_HOME: return "Home";
        case VK_END: return "End";
        case VK_INSERT: return "Insert";
        case VK_DELETE: return "Delete";
        case VK_PRIOR: return "PageUp";
        case VK_NEXT: return "PageDown";
        case VK_PAUSE: return "Pause";
        case VK_SNAPSHOT: return "PrintScreen";
        case VK_TAB: return "Tab";
        case VK_SPACE: return "Space";
        case VK_RETURN: return "Enter";
        case VK_ESCAPE: return "Esc";
        case VK_BACK: return "Backspace";
        case VK_CAPITAL: return "CapsLock";
        case VK_NUMPAD0: return "Num0";
        case VK_NUMPAD1: return "Num1";
        case VK_NUMPAD2: return "Num2";
        case VK_NUMPAD3: return "Num3";
        case VK_NUMPAD4: return "Num4";
        case VK_NUMPAD5: return "Num5";
        case VK_NUMPAD6: return "Num6";
        case VK_NUMPAD7: return "Num7";
        case VK_NUMPAD8: return "Num8";
        case VK_NUMPAD9: return "Num9";
        default:
            std::snprintf(text, sizeof(text), "VK %u", vk);
            return text;
        }
    }

    std::string hotkey_description(const hotkey &key)
    {
        if (key.vk == 0)
            return tr("未设置", "Not set");
        std::string out;
        if (key.ctrl) out += "Ctrl+";
        if (key.shift) out += "Shift+";
        if (key.alt) out += "Alt+";
        out += key_name(key.vk);
        return out;
    }

    void begin_hotkey_capture(int id)
    {
        g_hotkey_capture_id = id;
        g_hotkey_candidate = {};
    }

    void cancel_hotkey_capture()
    {
        g_hotkey_capture_id = 0;
        g_hotkey_candidate = {};
    }

    void collect_hotkey_candidate(effect_runtime *runtime)
    {
        if (g_hotkey_capture_id == 0)
            return;
        for (uint32_t vk = 7; vk < 256; ++vk)
        {
            if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_LCONTROL || vk == VK_RCONTROL ||
                vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_LMENU || vk == VK_RMENU)
                continue;
            if (!runtime->is_key_pressed(vk))
                continue;
            g_hotkey_candidate.vk = vk;
            g_hotkey_candidate.ctrl = runtime->is_key_down(VK_CONTROL);
            g_hotkey_candidate.shift = runtime->is_key_down(VK_SHIFT);
            g_hotkey_candidate.alt = runtime->is_key_down(VK_MENU);
            break;
        }
    }

    bool draw_hotkey_row(effect_runtime *runtime, const char *label, int id, hotkey &value)
    {
        ImGui::PushID(id);
        const bool capturing = g_hotkey_capture_id == id;
        std::string desc = capturing ? hotkey_description(g_hotkey_candidate) : hotkey_description(value);
        if (capturing && g_hotkey_candidate.vk == 0)
            desc = tr("按下快捷键…", "Press a shortcut...");

        ImGui::TextUnformatted(label);
        ImGui::SameLine(190.0f);
        char buffer[96] = {};
        std::snprintf(buffer, sizeof(buffer), "%s", desc.c_str());
        ImGui::SetNextItemWidth(190.0f);
        ImGui::InputText("##Key", buffer, sizeof(buffer), ImGuiInputTextFlags_ReadOnly);
        if (ImGui::IsItemClicked())
            begin_hotkey_capture(id);

        bool changed = false;
        if (capturing)
        {
            collect_hotkey_candidate(runtime);
            ImGui::SameLine();
            if (ImGui::SmallButton(tr("确定", "Apply")) && g_hotkey_candidate.vk != 0)
            {
                value = g_hotkey_candidate;
                cancel_hotkey_capture();
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(tr("取消", "Cancel")))
                cancel_hotkey_capture();
        }
        else
        {
            ImGui::SameLine();
            if (ImGui::SmallButton(tr("清除", "Clear")))
            {
                value = {};
                changed = true;
            }
        }
        ImGui::PopID();
        return changed;
    }

    void set_freeze_requested(runtime_state *state, bool freeze)
    {
        if (g_settings.strategy == capture_strategy::exact_snapshot_pair)
        {
            // Exact snapshot pairs are frozen by definition.
            g_frozen = false;
            g_freeze_armed = false;
            return;
        }

        if (!freeze)
        {
            g_frozen = false;
            g_freeze_armed = false;
        }
        else if (state != nullptr && state->pair_valid)
        {
            g_frozen = true;
            g_freeze_armed = false;
        }
        else
        {
            g_frozen = false;
            g_freeze_armed = true;
        }
        sync_ngx_capture_enabled();
    }

    void start_auto_sweep()
    {
        g_auto_sweep_active = true;
        if (g_settings.auto_reset_from_before)
        {
            g_settings.split_position = full_before_position();
            g_auto_direction = direction_toward_after();
        }
        else
        {
            g_auto_direction = g_settings.auto_sweep_direction;
        }
        mark_settings_dirty();
    }

    void toggle_auto_sweep()
    {
        if (g_auto_sweep_active)
            g_auto_sweep_active = false;
        else
            start_auto_sweep();
    }

    void process_move_key(effect_runtime *runtime, move_key_state &state, const hotkey &key, int direction, float dt, clock_type::time_point now)
    {
        const bool down = hotkey_down(runtime, key);
        if (down && !state.was_down)
        {
            state.pressed_at = now;
            state.long_mode = false;
        }
        else if (down && state.was_down)
        {
            const float held = std::chrono::duration<float>(now - state.pressed_at).count();
            if (held >= g_settings.hold_delay)
            {
                state.long_mode = true;
                g_auto_sweep_active = false;
                g_settings.split_position = std::clamp(
                    g_settings.split_position + direction * g_settings.move_speed * dt, 0.0f, 1.0f);
                mark_settings_dirty();
            }
        }
        else if (!down && state.was_down)
        {
            if (!state.long_mode)
            {
                g_auto_sweep_active = false;
                g_settings.split_position = std::clamp(
                    g_settings.split_position + direction * g_settings.move_step, 0.0f, 1.0f);
                mark_settings_dirty();
            }
            state.long_mode = false;
        }
        state.was_down = down;
    }

    void update_auto_sweep(float dt)
    {
        if (!g_auto_sweep_active)
            return;

        g_settings.split_position += static_cast<float>(g_auto_direction) * g_settings.auto_sweep_speed * dt;
        if (g_auto_direction > 0 && g_settings.split_position >= 1.0f)
        {
            g_settings.split_position = 1.0f;
            if (g_settings.auto_sweep_pingpong)
                g_auto_direction = -1;
            else
                g_auto_sweep_active = false;
        }
        else if (g_auto_direction < 0 && g_settings.split_position <= 0.0f)
        {
            g_settings.split_position = 0.0f;
            if (g_settings.auto_sweep_pingpong)
                g_auto_direction = 1;
            else
                g_auto_sweep_active = false;
        }
        mark_settings_dirty();
    }

    void update_screen_drag(effect_runtime *runtime, runtime_state *state)
    {
        if (!g_settings.screen_drag || !state->overlay_open)
        {
            state->divider_dragging = false;
            return;
        }

        HWND hwnd = static_cast<HWND>(runtime->get_hwnd());
        if (hwnd == nullptr)
            return;

        uint32_t mx = 0, my = 0;
        runtime->get_mouse_cursor_position(&mx, &my);
        POINT p { static_cast<LONG>(mx), static_cast<LONG>(my) };
        if (!ScreenToClient(hwnd, &p))
            return;

        RECT rc = {};
        if (!GetClientRect(hwnd, &rc))
            return;
        const float width = static_cast<float>(std::max<LONG>(1, rc.right - rc.left));
        const float divider_x = g_settings.split_position * width;
        const bool pressed = runtime->is_mouse_button_pressed(0);
        const bool down = runtime->is_mouse_button_down(0);

        if (pressed && std::abs(static_cast<float>(p.x) - divider_x) <= g_settings.drag_grab_px)
            state->divider_dragging = true;
        if (state->divider_dragging)
        {
            if (down)
            {
                g_auto_sweep_active = false;
                g_settings.split_position = std::clamp(static_cast<float>(p.x) / width, 0.0f, 1.0f);
                runtime->block_input_next_frame();
                mark_settings_dirty();
            }
            else
            {
                state->divider_dragging = false;
            }
        }
    }

    void arm_snapshot_capture(runtime_state *state, snapshot_target target)
    {
        if (state == nullptr || target == snapshot_target::none)
            return;
        state->pending_snapshot = target;
        if (target == snapshot_target::before)
            state->exact_before_valid = false;
        else
            state->exact_after_valid = false;
        state->pair_valid = false;
        state->logged_pair_ready = false;
        state->warned_incompatible_pair = false;
        g_frozen = false;
        g_freeze_armed = false;
        g_auto_sweep_active = false;
        state->capture_source = target == snapshot_target::before ? "Exact visible Before snapshot armed" : "Exact visible After snapshot armed";
        state->capture_note = "Capture waits until the ReShade settings overlay is closed, then copies the next visible backbuffer without FrameCompare OSD/composite.";
    }

    void update_hud_indicator_states(effect_runtime *runtime)
    {
        const auto now = clock_type::now();
        for (auto &item : g_settings.hud_indicators)
        {
            if (!item.enabled)
                continue;

            bool next = item.runtime_on;
            if (!item.runtime_initialized)
            {
                if (item.source == hud_state_source::reshade_effects_state)
                    next = runtime->get_effects_state();
                else if (item.source == hud_state_source::hotkey_hold)
                    next = hotkey_down(runtime, item.key);
                else if (item.source == hud_state_source::hotkey_pulse)
                    next = false;
                else
                    next = item.initial_on;
                item.runtime_on = next;
                item.runtime_initialized = true;
                // Pulse items should stay hidden until the first actual press.
                item.changed_at = item.source == hud_state_source::hotkey_pulse
                    ? clock_type::time_point::min()
                    : now;
            }

            if (item.source == hud_state_source::hotkey_pulse)
            {
                if (g_hotkey_capture_id == 0 && hotkey_pressed(runtime, item.key))
                {
                    item.runtime_on = true;
                    item.changed_at = now;
                }
                continue;
            }

            if (item.source == hud_state_source::reshade_effects_state)
                next = runtime->get_effects_state();
            else if (item.source == hud_state_source::hotkey_hold)
                next = hotkey_down(runtime, item.key);
            else if (g_hotkey_capture_id == 0 && hotkey_pressed(runtime, item.key))
                next = !item.runtime_on;

            if (next != item.runtime_on)
            {
                item.runtime_on = next;
                item.changed_at = now;
            }
        }
    }

    void update_hotkeys(effect_runtime *runtime, runtime_state *state)
    {
        if (!is_primary_runtime(runtime))
            return;

        const auto now = clock_type::now();
        float dt = std::chrono::duration<float>(now - state->last_tick).count();
        state->last_tick = now;
        dt = std::clamp(dt, 0.0f, 0.10f);

        update_hud_indicator_states(runtime);

        if (g_hotkey_capture_id != 0)
        {
            collect_hotkey_candidate(runtime);
            maybe_save_settings();
            return;
        }

        if (hotkey_pressed(runtime, g_settings.hk_toggle_compare))
        {
            g_settings.enabled = !g_settings.enabled;
            if (!g_settings.enabled)
            {
                g_auto_sweep_active = false;
                g_frozen = false;
                g_freeze_armed = false;
            }
            mark_settings_dirty();
            sync_ngx_capture_enabled();
        }

        if (g_settings.strategy == capture_strategy::exact_snapshot_pair)
        {
            if (hotkey_pressed(runtime, g_settings.hk_capture_before))
                arm_snapshot_capture(state, snapshot_target::before);
            if (hotkey_pressed(runtime, g_settings.hk_capture_after))
                arm_snapshot_capture(state, snapshot_target::after);
        }
        else if (hotkey_pressed(runtime, g_settings.hk_toggle_freeze))
        {
            set_freeze_requested(state, !(g_frozen || g_freeze_armed));
        }

        if (hotkey_pressed(runtime, g_settings.hk_toggle_auto))
            toggle_auto_sweep();
        if (hotkey_pressed(runtime, g_settings.hk_full_before))
        {
            g_auto_sweep_active = false;
            g_settings.split_position = full_before_position();
            mark_settings_dirty();
        }
        if (hotkey_pressed(runtime, g_settings.hk_full_after))
        {
            g_auto_sweep_active = false;
            g_settings.split_position = full_after_position();
            mark_settings_dirty();
        }

        process_move_key(runtime, state->left_key, g_settings.hk_left, -1, dt, now);
        process_move_key(runtime, state->right_key, g_settings.hk_right, 1, dt, now);
        update_auto_sweep(dt);
        update_screen_drag(runtime, state);
        maybe_save_settings();
    }

    resource make_native_resource(void *ptr)
    {
        return resource { reinterpret_cast<uintptr_t>(ptr) };
    }

    bool resolve_ngx_snapshot_resource(effect_runtime *runtime, runtime_state *state,
        const std::shared_ptr<const framecompare::ngx::snapshot> &snap,
        resource &out_resource, resource_usage &out_state, std::string &out_note)
    {
        out_resource = {};
        out_state = resource_usage::copy_source;
        if (!snap || !snap->native)
            return false;

        device *dev = runtime->get_device();
        const auto api = dev->get_api();
        const uint64_t native_device = dev->get_native();
        const auto &n = *snap->native;

        state->active_snapshot = snap;
        if (n.api == framecompare::ngx::snapshot_api::d3d11 && api == device_api::d3d11)
        {
            if (reinterpret_cast<uint64_t>(n.device11.Get()) != native_device)
            {
                out_note = "NGX D3D11 snapshot belongs to a different device; safe cross-device D3D11 import is unavailable.";
                return false;
            }
            out_resource = make_native_resource(n.texture11.Get());
            out_note = "NGX Feature 18 D3D11 Color (pre-Evaluate)";
            return out_resource != 0;
        }

        if (n.api == framecompare::ngx::snapshot_api::d3d12 && api == device_api::d3d12)
        {
            if (reinterpret_cast<uint64_t>(n.device12.Get()) == native_device)
            {
                out_resource = make_native_resource(n.texture12.Get());
                out_note = "NGX Feature 18 D3D12 Color (pre-Evaluate)";
                return out_resource != 0;
            }

            if (!g_settings.allow_unsafe_cross_device_ngx || !n.cross_device_shareable || n.shared_handle == nullptr)
            {
                out_note = "NGX uses a different D3D12 device. Cross-device capture is disabled because queue synchronization cannot be guaranteed.";
                return false;
            }

            ID3D12Device *runtime_device = reinterpret_cast<ID3D12Device *>(native_device);
            if (state->imported_identity != &n)
            {
                state->imported12.Reset();
                if (!runtime_device || FAILED(runtime_device->OpenSharedHandle(n.shared_handle, IID_PPV_ARGS(&state->imported12))))
                {
                    out_note = "Unable to open NGX D3D12 shared snapshot on the ReShade D3D12 device.";
                    return false;
                }
                state->imported_identity = &n;
            }
            out_resource = make_native_resource(state->imported12.Get());
            out_note = "NGX Feature 18 cross-device D3D12 snapshot (UNSAFE opt-in; synchronization not guaranteed)";
            return out_resource != 0;
        }

        if (n.api == framecompare::ngx::snapshot_api::d3d12 && api == device_api::d3d11)
        {
            if (!g_settings.allow_unsafe_cross_device_ngx || !n.cross_device_shareable || n.shared_handle == nullptr)
            {
                out_note = "NGX Feature 18 is on a private D3D12 device while ReShade is D3D11. Unsafe cross-API import is disabled.";
                return false;
            }

            ID3D11Device *runtime_device = reinterpret_cast<ID3D11Device *>(native_device);
            ComPtr<ID3D11Device1> device1;
            if (!runtime_device || FAILED(runtime_device->QueryInterface(IID_PPV_ARGS(&device1))))
            {
                out_note = "ReShade D3D11 device does not expose ID3D11Device1 for shared NT handle import.";
                return false;
            }
            if (state->imported_identity != &n)
            {
                state->imported11.Reset();
                if (FAILED(device1->OpenSharedResource1(n.shared_handle, IID_PPV_ARGS(&state->imported11))))
                {
                    out_note = "Unable to open NGX D3D12 shared texture from D3D11.";
                    return false;
                }
                state->imported_identity = &n;
            }
            out_resource = make_native_resource(state->imported11.Get());
            out_note = "NGX Feature 18 D3D12->D3D11 shared snapshot (UNSAFE opt-in; synchronization not guaranteed)";
            return out_resource != 0;
        }

        out_note = "NGX snapshot API does not match the ReShade runtime API.";
        return false;
    }

    bool capture_latest_ngx(effect_runtime *runtime, runtime_state *state, command_list *cmd)
    {
        const auto snap = framecompare::ngx::latest_snapshot();
        if (!snap || !snap->native || snap->generation == state->last_ngx_generation)
        {
            state->latest_ngx_age_ms = -1.0;
            return false;
        }

        const auto age = std::chrono::duration<double, std::milli>(clock_type::now() - snap->captured_at).count();
        state->latest_ngx_age_ms = age;
        if (age < 0.0 || age > static_cast<double>(g_settings.ngx_max_age_ms))
        {
            state->capture_note = "Newest NGX Feature 18 snapshot is too old for this frame.";
            return false;
        }

        resource source = {};
        resource_usage source_state = resource_usage::copy_source;
        std::string note;
        if (!resolve_ngx_snapshot_resource(runtime, state, snap, source, source_state, note))
        {
            state->capture_note = std::move(note);
            return false;
        }

        if (!copy_into_capture(runtime, cmd, source, source_state, state->before, "FrameCompare Before (NGX)"))
        {
            state->capture_note = "NGX snapshot was found but could not be copied into the comparison texture.";
            return false;
        }

        state->last_ngx_generation = snap->generation;
        state->before_this_cycle = true;
        state->before_width = state->before.width;
        state->before_height = state->before.height;
        state->capture_source = note;
        state->capture_note.clear();
        return true;
    }

    bool prepare_compositor(effect_runtime *runtime, runtime_state *state, command_list *cmd)
    {
        if (!state->pair_valid || !state->before.ready || !state->after.ready)
            return false;

        if (state->composite == 0)
        {
            refresh_effect_handles(runtime, state);
            if (state->composite == 0)
            {
                if (!state->warned_missing_fx)
                {
                    g_logged_fx_ready.store(false);
                    if (!g_logged_fx_missing.exchange(true))
                        fc_log(reshade::log::level::warning,
                            "未找到 FrameCompare.fx / FrameCompareComposite。请把 FrameCompare.fx 放到 reshade-shaders\\Shaders\\ 后重新加载效果。",
                            "FrameCompare.fx / FrameCompareComposite was not found. Put FrameCompare.fx in reshade-shaders\\Shaders\\ and reload effects.");
                    state->warned_missing_fx = true;
                }
                return false;
            }
        }

        if (!update_parameter_texture(runtime, state, cmd))
        {
            if (!state->warned_param_upload)
            {
                fc_log(reshade::log::level::error,
                    "当前图形后端不支持参数纹理上传，FrameCompare 合成无法继续。",
                    "This graphics backend does not support the parameter-texture upload required by FrameCompare.");
                state->warned_param_upload = true;
            }
            return false;
        }

        runtime->update_texture_bindings("FRAMECOMPARE_BEFORE", state->before.srv, state->before.srv);
        runtime->update_texture_bindings("FRAMECOMPARE_AFTER", state->after.srv, state->after.srv);
        runtime->update_texture_bindings("FRAMECOMPARE_PARAMS", state->params.srv, state->params.srv);
        return true;
    }

    void on_init_runtime(effect_runtime *runtime)
    {
        auto *state = runtime->create_private_data<runtime_state>();
        state->last_tick = clock_type::now();
        {
            std::lock_guard lock(g_runtime_mutex);
            if (g_primary_runtime == nullptr)
                g_primary_runtime = runtime;
        }
        // Effects may still be compiling at init_runtime. Do not diagnose a missing
        // FrameCompare.fx here; reshade_reloaded_effects is the first reliable check.
        refresh_effect_handles(runtime, state);
    }

    void on_destroy_runtime(effect_runtime *runtime)
    {
        {
            std::lock_guard lock(g_runtime_mutex);
            if (g_primary_runtime == runtime)
                g_primary_runtime = nullptr;
        }

        if (auto *state = runtime->get_private_data<runtime_state>())
        {
            runtime->get_command_queue()->wait_idle();
            device *dev = runtime->get_device();
            destroy_gpu_texture(dev, state->before);
            destroy_gpu_texture(dev, state->after);
            destroy_parameter_texture(dev, state->params);
            state->active_snapshot.reset();
            state->imported11.Reset();
            state->imported12.Reset();
            runtime->destroy_private_data<runtime_state>();
        }
    }

    void on_reloaded_effects(effect_runtime *runtime)
    {
        if (auto *state = runtime->get_private_data<runtime_state>())
        {
            state->warned_missing_fx = false;
            refresh_effect_handles(runtime, state);
            if (state->composite == 0)
            {
                g_logged_fx_ready.store(false);
                if (!g_logged_fx_missing.exchange(true))
                    fc_log(reshade::log::level::warning,
                        "重新加载效果后仍未找到 FrameCompare.fx / FrameCompareComposite。",
                        "FrameCompare.fx / FrameCompareComposite is still missing after effect reload.");
                state->warned_missing_fx = true;
            }
        }
    }

    void on_begin_effects(effect_runtime *runtime, command_list *cmd, resource_view rtv, resource_view)
    {
        auto *state = runtime->get_private_data<runtime_state>();
        if (!state)
            return;

        // The compositor is rendered explicitly at finish_effects only. Keep it out
        // of the normal preset technique order.
        if (state->composite != 0)
            runtime->set_technique_state(state->composite, false);

        if (g_settings.strategy != capture_strategy::live_pipeline || !g_settings.enabled || g_frozen ||
            state->pending_snapshot != snapshot_target::none)
            return;

        ++state->effect_cycle;
        state->fallback_before_this_cycle = false;
        state->before_this_cycle = false;

        // Live Auto keeps a same-frame pre-ReShade fallback. A fresh Feature 18
        // snapshot may overwrite it in finish_effects.
        if (g_settings.capture == capture_mode::automatic || g_settings.capture == capture_mode::before_reshade_fx)
        {
            const resource target = runtime->get_device()->get_resource_from_view(rtv);
            if (copy_into_capture(runtime, cmd, target, resource_usage::render_target, state->before, "FrameCompare Before (PreFX)"))
            {
                state->fallback_before_this_cycle = true;
                state->before_this_cycle = true;
                state->before_width = state->before.width;
                state->before_height = state->before.height;
                state->capture_source = g_settings.capture == capture_mode::before_reshade_fx
                    ? "Immediately before ReShade FX"
                    : "Before ReShade FX (Auto fallback)";
                state->capture_note.clear();
            }
        }
    }

    void on_finish_effects(effect_runtime *runtime, command_list *cmd, resource_view rtv, resource_view rtv_srgb)
    {
        auto *state = runtime->get_private_data<runtime_state>();
        if (!state)
            return;

        // Exact visible snapshots are taken later at reshade_present. Do not let an
        // old comparison image contaminate the frame being captured.
        if (state->pending_snapshot != snapshot_target::none)
            return;

        if (g_settings.strategy == capture_strategy::live_pipeline && g_settings.enabled && !g_frozen)
        {
            if (g_settings.capture == capture_mode::automatic || g_settings.capture == capture_mode::ngx_feature18)
            {
                const bool got_ngx = capture_latest_ngx(runtime, state, cmd);
                if (!got_ngx && g_settings.capture == capture_mode::ngx_feature18)
                {
                    state->before_this_cycle = false;
                    state->capture_source = "NGX Feature 18 strict mode: no fresh usable snapshot";
                }
                else if (!got_ngx && g_settings.capture == capture_mode::automatic)
                {
                    state->before_this_cycle = state->fallback_before_this_cycle;
                }
            }

            if (state->before_this_cycle)
            {
                const resource target = runtime->get_device()->get_resource_from_view(rtv);
                if (copy_into_capture(runtime, cmd, target, resource_usage::render_target, state->after, "FrameCompare After"))
                {
                    state->after_width = state->after.width;
                    state->after_height = state->after.height;
                    update_pair_validity(state, false);
                    if (state->pair_valid && !state->logged_pair_ready)
                    {
                        const std::string dims = std::to_string(state->before_width) + "x" + std::to_string(state->before_height) +
                            " / " + std::to_string(state->after_width) + "x" + std::to_string(state->after_height);
                        fc_log(reshade::log::level::info,
                            "Before/After 画面对已就绪：" + dims + "；捕获来源：" + localized_runtime_text(state->capture_source),
                            "Before/After pair is ready: " + dims + "; capture source: " + state->capture_source);
                        state->logged_pair_ready = true;
                    }
                    if (g_freeze_armed && state->pair_valid)
                    {
                        g_freeze_armed = false;
                        g_frozen = true;
                        sync_ngx_capture_enabled();
                    }
                }
            }
        }

        if (!g_settings.enabled || !state->pair_valid || !state->before.ready || !state->after.ready)
            return;
        if (!prepare_compositor(runtime, state, cmd))
            return;

        runtime->render_technique(state->composite, cmd, rtv, rtv_srgb);
    }

    void emit_runtime_log_diagnostics(runtime_state *state)
    {
        if (g_settings.verbose_logging && !state->capture_source.empty() &&
            state->capture_source != state->last_logged_capture_source)
        {
            const std::string zh = "捕获来源变化：" + localized_runtime_text(state->capture_source);
            const std::string en = "Capture source changed: " + state->capture_source;
            fc_log(reshade::log::level::info, zh, en);
            state->last_logged_capture_source = state->capture_source;
        }

        const auto diag = framecompare::ngx::get_diagnostics();
        if (!diag.last_error.empty() && diag.last_error != state->last_logged_ngx_error)
        {
            const std::string zh = "NGX 捕获错误：" + diag.last_error;
            const std::string en = "NGX capture error: " + diag.last_error;
            fc_log(reshade::log::level::error, zh, en);
            state->last_logged_ngx_error = diag.last_error;
        }
        if (diag.failed_captures != state->last_logged_ngx_failures)
        {
            if (g_settings.verbose_logging && diag.failed_captures > state->last_logged_ngx_failures)
            {
                const std::string count = std::to_string(diag.failed_captures);
                fc_log(reshade::log::level::warning,
                    "NGX 捕获失败累计次数：" + count,
                    "NGX capture failure count: " + count);
            }
            state->last_logged_ngx_failures = diag.failed_captures;
        }
    }

    bool capture_pending_exact_snapshot(effect_runtime *runtime, runtime_state *state)
    {
        if (g_settings.strategy != capture_strategy::exact_snapshot_pair ||
            state->pending_snapshot == snapshot_target::none || state->overlay_open)
            return false;

        command_queue *queue = runtime->get_command_queue();
        command_list *cmd = queue != nullptr ? queue->get_immediate_command_list() : nullptr;
        const resource backbuffer = runtime->get_current_back_buffer();
        const snapshot_target target = state->pending_snapshot;
        gpu_texture &dest = target == snapshot_target::before ? state->before : state->after;
        const char *debug_name = target == snapshot_target::before
            ? "FrameCompare Exact Before"
            : "FrameCompare Exact After";

        if (!copy_into_capture(runtime, cmd, backbuffer, resource_usage::present, dest, debug_name))
        {
            state->capture_note = "Exact visible snapshot copy failed; request remains armed for the next frame.";
            return false;
        }

        if (target == snapshot_target::before)
        {
            state->exact_before_valid = true;
            state->before_width = dest.width;
            state->before_height = dest.height;
            state->capture_source = "Exact visible Before snapshot";
        }
        else
        {
            state->exact_after_valid = true;
            state->after_width = dest.width;
            state->after_height = dest.height;
            state->capture_source = "Exact visible After snapshot";
        }

        state->pending_snapshot = snapshot_target::none;
        update_pair_validity(state, true);
        if (state->pair_valid)
            state->capture_note = "Exact snapshot pair ready. Divider motion does not re-render either side.";
        else if (!(state->exact_before_valid && state->exact_after_valid))
            state->capture_note = "Snapshot saved. Capture the other side to complete the exact pair.";

        const std::string side = target == snapshot_target::before ? "Before" : "After";
        fc_log(reshade::log::level::info,
            "精确 " + side + " 截图已捕获。" + (state->pair_valid ? std::string(" Before/After 画面对已就绪。") : std::string()),
            "Exact " + side + " snapshot captured." + (state->pair_valid ? std::string(" Before/After pair is ready.") : std::string()));
        return true;
    }

    void draw_osd_text(const char *text, float anchor_x, float anchor_y)
    {
        if (text == nullptr || *text == '\0')
            return;

        const ImGuiIO &io = ImGui::GetIO();
        const float margin = std::max(0.0f, g_settings.osd_margin_px);
        ImFont *font = ImGui::GetFont();
        ImGui::PushFont(font, static_cast<float>(g_settings.font_size_px));
        const ImVec2 size = ImGui::CalcTextSize(text);

        const float ax = std::clamp(anchor_x, 0.0f, 1.0f);
        const float ay = std::clamp(anchor_y, 0.0f, 1.0f);
        float x = margin + ax * std::max(0.0f, io.DisplaySize.x - margin * 2.0f);
        float y = margin + ay * std::max(0.0f, io.DisplaySize.y - margin * 2.0f);
        x -= ax * size.x;
        y -= ay * size.y;
        x = std::clamp(x, margin, std::max(margin, io.DisplaySize.x - margin - size.x));
        y = std::clamp(y, margin, std::max(margin, io.DisplaySize.y - margin - size.y));

        const float outline = std::max(0.0f, g_settings.outline_px);
        const float alpha = std::clamp(g_settings.label_opacity, 0.0f, 1.0f);
        if (outline > 0.0f)
        {
            const ImVec4 black(0.0f, 0.0f, 0.0f, alpha);
            const ImVec2 offsets[] = {
                { -outline, 0.0f }, { outline, 0.0f }, { 0.0f, -outline }, { 0.0f, outline },
                { -outline, -outline }, { outline, -outline }, { -outline, outline }, { outline, outline }
            };
            for (const ImVec2 &d : offsets)
            {
                ImGui::SetCursorScreenPos(ImVec2(x + d.x, y + d.y));
                ImGui::TextColored(black, "%s", text);
            }
        }

        ImGui::SetCursorScreenPos(ImVec2(x, y));
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, alpha), "%s", text);
        ImGui::PopFont();
    }

    void on_reshade_overlay(effect_runtime *runtime)
    {
        if (!is_primary_runtime(runtime))
            return;
        auto *state = runtime->get_private_data<runtime_state>();
        if (!state)
            return;

        // Pending exact capture must see the game image, not FrameCompare's own OSD.
        if (state->pending_snapshot != snapshot_target::none && !state->overlay_open)
            return;

        bool has_anything = false;
        const bool show_pair_labels = g_settings.label_preview ||
            (g_settings.show_labels && g_settings.enabled && state->pair_valid);
        has_anything |= show_pair_labels;
        for (const auto &item : g_settings.hud_indicators)
            has_anything |= item.enabled;
        if (!has_anything)
            return;

        const ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus;
        if (!ImGui::Begin("##FrameCompareOSDv13", nullptr, flags))
        {
            ImGui::End();
            return;
        }

        if (show_pair_labels)
        {
            draw_osd_text(g_settings.before_text.data(), g_settings.before_x, g_settings.before_y);
            draw_osd_text(g_settings.after_text.data(), g_settings.after_x, g_settings.after_y);
        }

        const auto now = clock_type::now();
        for (const auto &item : g_settings.hud_indicators)
        {
            if (!item.enabled || !item.runtime_initialized)
                continue;
            if (item.source == hud_state_source::hotkey_pulse && item.changed_at == clock_type::time_point::min())
                continue;
            if (item.show_seconds > 0.0f && std::chrono::duration<float>(now - item.changed_at).count() > item.show_seconds)
                continue;
            draw_osd_text(item.source == hud_state_source::hotkey_pulse
                    ? item.text_on.data()
                    : (item.runtime_on ? item.text_on.data() : item.text_off.data()),
                item.x, item.y);
        }

        ImGui::End();
    }

    void on_reshade_present(effect_runtime *runtime)
    {
        if (auto *state = runtime->get_private_data<runtime_state>())
        {
            capture_pending_exact_snapshot(runtime, state);
            update_hotkeys(runtime, state);
            emit_runtime_log_diagnostics(state);
        }
        sync_ngx_capture_enabled();
    }

    bool on_open_overlay(effect_runtime *runtime, bool open, input_source)
    {
        if (auto *state = runtime->get_private_data<runtime_state>())
        {
            state->overlay_open = open;
            if (!open)
                state->divider_dragging = false;
        }
        return false;
    }

    const char *capture_mode_name(capture_mode mode)
    {
        switch (mode)
        {
        case capture_mode::automatic:
            return tr("自动：优先 NGX Feature 18，失败时回退到 ReShade FX 前",
                      "Auto: NGX Feature 18, fallback to Before ReShade FX");
        case capture_mode::ngx_feature18:
            return tr("严格 NGX Feature 18（Evaluate 前 Color）",
                      "NGX Feature 18 strict (pre-Evaluate Color)");
        case capture_mode::before_reshade_fx:
            return tr("ReShade FX 前（推荐 Feeder / 效果链注入）",
                      "Before ReShade FX (recommended for Feeder/effect-chain injection)");
        }
        return tr("未知", "Unknown");
    }

    const char *api_name(framecompare::ngx::snapshot_api api)
    {
        switch (api)
        {
        case framecompare::ngx::snapshot_api::d3d11: return "D3D11";
        case framecompare::ngx::snapshot_api::d3d12: return "D3D12";
        default: return "None";
        }
    }

    const char *strategy_name(capture_strategy strategy)
    {
        switch (strategy)
        {
        case capture_strategy::exact_snapshot_pair: return tr("精确截图对", "Exact snapshot pair");
        case capture_strategy::live_pipeline: return tr("实时处理链", "Live pipeline");
        }
        return tr("未知", "Unknown");
    }

    const char *hud_source_name(hud_state_source source)
    {
        switch (source)
        {
        case hud_state_source::hotkey_toggle: return tr("快捷键镜像开关", "Hotkey mirror toggle");
        case hud_state_source::reshade_effects_state: return tr("ReShade 实际效果状态", "Actual ReShade effects state");
        case hud_state_source::hotkey_hold: return tr("按住快捷键", "Hotkey hold");
        }
        return tr("未知", "Unknown");
    }

    void draw_settings(effect_runtime *runtime)
    {
        if (!is_primary_runtime(runtime))
            return;
        auto *state = runtime->get_private_data<runtime_state>();
        if (!state)
            return;

        bool changed = false;

        const char *language_items[] = { "中文", "English" };
        int language = static_cast<int>(g_settings.language);
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::Combo(tr("界面语言##Language", "UI language##Language"), &language, language_items, 2))
        {
            g_settings.language = static_cast<ui_language>(language);
            changed = true;
        }

        ImGui::TextDisabled("FrameCompare v1.3.0");
        ImGui::SameLine();
        ImGui::TextDisabled("| %s", strategy_name(g_settings.strategy));
        ImGui::Separator();

        if (ImGui::CollapsingHeader(tr("快速流程 / 使用说明##QuickStart", "Quick workflow / usage##QuickStart"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            const std::string capture_before_key = hotkey_description(g_settings.hk_capture_before);
            const std::string capture_after_key = hotkey_description(g_settings.hk_capture_after);
            const std::string compare_key = hotkey_description(g_settings.hk_toggle_compare);
            const std::string sweep_key = hotkey_description(g_settings.hk_toggle_auto);
            ImGui::TextWrapped(tr(
                "推荐录制流程：选择“精确截图对” → 把游戏/插件切到真正 OFF → [%s] 捕获 Before → 切到完整增强 ON → [%s] 捕获 After → [%s] 开启对比 → 使用移动快捷键、[%s] 自动扫屏或鼠标拖动分割线。捕获请求会等到 ReShade 设置面板关闭后再抓下一帧，避免把 FrameCompare 自己的界面录进去。",
                "Recommended workflow: Exact snapshot pair -> switch the game/plugins to the real OFF state -> [%s] captures Before -> switch to full enhanced ON -> [%s] captures After -> [%s] enables comparison -> use move hotkeys, [%s] auto sweep, or mouse drag. Capture requests wait until the ReShade settings overlay is closed before grabbing the next frame, so FrameCompare's own UI is not captured."),
                capture_before_key.c_str(), capture_after_key.c_str(), compare_key.c_str(), sweep_key.c_str());
            ImGui::TextWrapped("%s", tr(
                "精确截图对是真正的两张可见画面，适合静态机位和录制扫屏。实时处理链用于动态画面，但 Before 是否等于绝对原版取决于 DLSS/RenoDX/游戏的注入位置。",
                "Exact snapshot pair stores two actually visible frames and is intended for static-shot recording. Live pipeline is for moving scenes, but whether Before is absolute vanilla depends on the game's DLSS/RenoDX injection point."));
        }

        if (ImGui::CollapsingHeader(tr("对比与捕获##Compare", "Comparison & capture##Compare"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::Checkbox(tr("启用对比画面##Enabled", "Enable comparison image##Enabled"), &g_settings.enabled))
            {
                if (!g_settings.enabled)
                {
                    g_auto_sweep_active = false;
                    g_frozen = false;
                    g_freeze_armed = false;
                }
                changed = true;
                sync_ngx_capture_enabled();
            }

            const char *strategy_zh[] = { "精确截图对（推荐录制）", "实时处理链（高级）" };
            const char *strategy_en[] = { "Exact snapshot pair (recommended for recording)", "Live pipeline (advanced)" };
            int strategy = static_cast<int>(g_settings.strategy);
            if (ImGui::Combo(tr("工作模式##Strategy", "Workflow##Strategy"), &strategy,
                    g_settings.language == ui_language::chinese ? strategy_zh : strategy_en, 2))
            {
                g_settings.strategy = static_cast<capture_strategy>(strategy);
                state->pair_valid = false;
                state->exact_before_valid = false;
                state->exact_after_valid = false;
                state->pending_snapshot = snapshot_target::none;
                state->logged_pair_ready = false;
                state->warned_incompatible_pair = false;
                g_frozen = false;
                g_freeze_armed = false;
                g_auto_sweep_active = false;
                changed = true;
                sync_ngx_capture_enabled();
            }

            if (g_settings.strategy == capture_strategy::exact_snapshot_pair)
            {
                ImGui::TextWrapped("%s", tr(
                    "Before / After 由你实际切换 OFF/ON 后分别捕获。插件不会假装同一动态帧能同时拥有所有第三方插件的“真正原版”和“最终增强”状态。",
                    "Capture Before and After after you actually switch the enhancement stack OFF/ON. The add-on does not pretend that every third-party injection can provide true vanilla and final-enhanced images from the same moving frame."));

                if (ImGui::Button(tr("捕获 Before##CaptureBefore", "Capture Before##CaptureBefore")))
                    arm_snapshot_capture(state, snapshot_target::before);
                ImGui::SameLine();
                ImGui::TextDisabled("[%s]", hotkey_description(g_settings.hk_capture_before).c_str());
                ImGui::SameLine();
                if (ImGui::Button(tr("捕获 After##CaptureAfter", "Capture After##CaptureAfter")))
                    arm_snapshot_capture(state, snapshot_target::after);
                ImGui::SameLine();
                ImGui::TextDisabled("[%s]", hotkey_description(g_settings.hk_capture_after).c_str());
                ImGui::SameLine();
                if (ImGui::Button(tr("清空截图对##ClearPair", "Clear pair##ClearPair")))
                {
                    state->pair_valid = false;
                    state->exact_before_valid = false;
                    state->exact_after_valid = false;
                    state->pending_snapshot = snapshot_target::none;
                    state->before.ready = false;
                    state->after.ready = false;
                    state->logged_pair_ready = false;
                    state->warned_incompatible_pair = false;
                    state->capture_source = "None";
                    state->capture_note.clear();
                }

                ImGui::Text("Before: %s | After: %s | %s: %s",
                    state->exact_before_valid ? tr("已捕获", "captured") : tr("未捕获", "not captured"),
                    state->exact_after_valid ? tr("已捕获", "captured") : tr("未捕获", "not captured"),
                    tr("画面对", "Pair"), state->pair_valid ? tr("已就绪", "ready") : tr("未就绪", "not ready"));
                if (state->pending_snapshot != snapshot_target::none)
                    ImGui::TextDisabled("%s", tr("捕获已准备：关闭 ReShade 面板后会自动抓取下一帧。", "Capture armed: close the ReShade overlay and the next frame will be grabbed automatically."));
            }
            else
            {
                const char *capture_zh[] = {
                    "自动：优先 NGX Feature 18，失败回退到 ReShade FX 前",
                    "严格 NGX Feature 18",
                    "ReShade FX 前"
                };
                const char *capture_en[] = {
                    "Auto: prefer NGX Feature 18, fallback to pre-ReShade FX",
                    "Strict NGX Feature 18",
                    "Before ReShade FX"
                };
                int capture = static_cast<int>(g_settings.capture);
                if (ImGui::Combo(tr("实时 Before 来源##LiveCapture", "Live Before source##LiveCapture"), &capture,
                        g_settings.language == ui_language::chinese ? capture_zh : capture_en, 3))
                {
                    g_settings.capture = static_cast<capture_mode>(capture);
                    state->pair_valid = false;
                    state->logged_pair_ready = false;
                    state->warned_incompatible_pair = false;
                    changed = true;
                    sync_ngx_capture_enabled();
                }
                ImGui::TextWrapped("%s", tr(
                    "这里仅决定实时模式从处理链哪个阶段取 Before。它不再作为主界面的四个难懂“捕获模式”暴露；Application Present 模式已移除。",
                    "This only selects where Live mode gets its Before image. The old four-way capture-mode UI is gone, and Application Present mode has been removed."));

                bool freeze_value = g_frozen || g_freeze_armed;
                if (ImGui::Checkbox(tr("冻结当前实时画面对##Freeze", "Freeze current live pair##Freeze"), &freeze_value))
                    set_freeze_requested(state, freeze_value);
                if (g_freeze_armed)
                    ImGui::SameLine(), ImGui::TextDisabled("%s", tr("等待首个有效画面对", "waiting for first valid pair"));
            }

            changed |= ImGui::Checkbox(tr("Before 在左侧##BeforeLeft", "Before on left##BeforeLeft"), &g_settings.before_on_left);

            const char *display_zh[] = { "普通同坐标擦除（备用）", "SplitScreenCR 分屏（推荐）" };
            const char *display_en[] = { "Normal same-coordinate wipe (fallback)", "SplitScreenCR split (recommended)" };
            int display = static_cast<int>(g_settings.display);
            if (ImGui::Combo(tr("显示方式##Display", "Display mode##Display"), &display,
                    g_settings.language == ui_language::chinese ? display_zh : display_en, 2))
            {
                g_settings.display = static_cast<display_mode>(display);
                changed = true;
            }
        }

        if (ImGui::CollapsingHeader(tr("分割线与动画##Divider", "Divider & animation##Divider"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            changed |= ImGui::SliderFloat(tr("分割位置##Position", "Split position##Position"), &g_settings.split_position, 0.0f, 1.0f, "%.3f");
            changed |= ImGui::Checkbox(tr("显示分割线##ShowBorder", "Show divider##ShowBorder"), &g_settings.show_border);
            changed |= ImGui::SliderFloat(tr("分割线宽度##BorderWidth", "Divider width##BorderWidth"), &g_settings.border_width, 0.0f, 0.02f, "%.4f");
            changed |= ImGui::SliderFloat(tr("分割线透明度##BorderOpacity", "Divider opacity##BorderOpacity"), &g_settings.border_opacity, 0.0f, 1.0f, "%.2f");
            changed |= ImGui::SliderFloat(tr("短按移动步长##MoveStep", "Short-press step##MoveStep"), &g_settings.move_step, 0.001f, 0.20f, "%.3f");
            changed |= ImGui::SliderFloat(tr("长按启动延迟（秒）##HoldDelay", "Hold delay (seconds)##HoldDelay"), &g_settings.hold_delay, 0.0f, 1.0f, "%.2f");
            changed |= ImGui::SliderFloat(tr("长按移动速度（屏/秒）##MoveSpeed", "Hold move speed (screen/sec)##MoveSpeed"), &g_settings.move_speed, 0.01f, 2.0f, "%.2f");

            ImGui::SeparatorText(tr("自动扫屏", "Auto sweep"));
            if (ImGui::Button(g_auto_sweep_active ? tr("停止自动扫屏##Sweep", "Stop auto sweep##Sweep") : tr("启动自动扫屏##Sweep", "Start auto sweep##Sweep")))
                toggle_auto_sweep();
            ImGui::SameLine();
            ImGui::TextDisabled("[%s]", hotkey_description(g_settings.hk_toggle_auto).c_str());
            changed |= ImGui::SliderFloat(tr("扫屏速度（屏/秒）##SweepSpeed", "Sweep speed (screen/sec)##SweepSpeed"), &g_settings.auto_sweep_speed, 0.01f, 2.0f, "%.2f");
            changed |= ImGui::Checkbox(tr("到边界后往返##PingPong", "Ping-pong at edges##PingPong"), &g_settings.auto_sweep_pingpong);
            changed |= ImGui::Checkbox(tr("启动时从 Before 全屏开始##ResetBefore", "Start from full Before##ResetBefore"), &g_settings.auto_reset_from_before);
            int direction = g_settings.auto_sweep_direction;
            const char *dirs_zh[] = { "向左", "向右" };
            const char *dirs_en[] = { "Left", "Right" };
            int dir_index = direction < 0 ? 0 : 1;
            if (ImGui::Combo(tr("非重置时初始方向##SweepDirection", "Initial direction when not resetting##SweepDirection"), &dir_index,
                    g_settings.language == ui_language::chinese ? dirs_zh : dirs_en, 2))
            {
                g_settings.auto_sweep_direction = dir_index == 0 ? -1 : 1;
                changed = true;
            }

            ImGui::SeparatorText(tr("鼠标拖动", "Mouse drag"));
            changed |= ImGui::Checkbox(tr("ReShade 面板打开时允许拖动分割线##ScreenDrag", "Allow divider drag while ReShade overlay is open##ScreenDrag"), &g_settings.screen_drag);
            changed |= ImGui::SliderFloat(tr("拖动捕获宽度（px）##DragGrab", "Drag grab width (px)##DragGrab"), &g_settings.drag_grab_px, 2.0f, 50.0f, "%.0f px");
        }

        if (ImGui::CollapsingHeader(tr("文字标签##Labels", "Labels##Labels"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            changed |= ImGui::Checkbox(tr("显示 Before / After 标签##ShowLabels", "Show Before / After labels##ShowLabels"), &g_settings.show_labels);
            changed |= ImGui::Checkbox(tr("位置预览（没有画面对也显示）##PreviewLabels", "Position preview (show without pair)##PreviewLabels"), &g_settings.label_preview);
            changed |= ImGui::InputText(tr("Before 文字##BeforeText", "Before text##BeforeText"), g_settings.before_text.data(), g_settings.before_text.size());
            changed |= ImGui::InputText(tr("After 文字##AfterText", "After text##AfterText"), g_settings.after_text.data(), g_settings.after_text.size());
            changed |= ImGui::SliderFloat(tr("Before X##BeforeX", "Before X##BeforeX"), &g_settings.before_x, 0.0f, 1.0f, "%.3f");
            changed |= ImGui::SliderFloat(tr("Before Y##BeforeY", "Before Y##BeforeY"), &g_settings.before_y, 0.0f, 1.0f, "%.3f");
            changed |= ImGui::SliderFloat(tr("After X##AfterX", "After X##AfterX"), &g_settings.after_x, 0.0f, 1.0f, "%.3f");
            changed |= ImGui::SliderFloat(tr("After Y##AfterY", "After Y##AfterY"), &g_settings.after_y, 0.0f, 1.0f, "%.3f");
            changed |= ImGui::SliderInt(tr("全局字体大小##FontSize", "Global font size##FontSize"), &g_settings.font_size_px, 12, 128);
            changed |= ImGui::SliderFloat(tr("全局文字透明度##LabelOpacity", "Global text opacity##LabelOpacity"), &g_settings.label_opacity, 0.0f, 1.0f, "%.2f");
            changed |= ImGui::SliderFloat(tr("全局描边##Outline", "Global outline##Outline"), &g_settings.outline_px, 0.0f, 8.0f, "%.1f px");
            changed |= ImGui::SliderFloat(tr("屏幕安全边距##Margin", "Screen safe margin##Margin"), &g_settings.osd_margin_px, 0.0f, 100.0f, "%.0f px");
            ImGui::TextWrapped("%s", tr(
                "v1.3 不再用 GDI 先生成文字纹理，而是直接在 ReShade 的 ImGui OSD 中绘字，并按完整文字尺寸限制到屏幕安全区，因此 ON/OFF 顶部和底部不会再被旧纹理边界裁掉。X/Y 是锚点：0=左/上，1=右/下。",
                "v1.3 no longer rasterizes labels into GDI textures. Text is drawn in ReShade's ImGui OSD and clamped by its full measured size, preventing the old top/bottom glyph clipping. X/Y are anchors: 0=left/top, 1=right/bottom."));
        }

        if (ImGui::CollapsingHeader(tr("自定义 HUD##HUD", "Custom HUD##HUD"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextWrapped("%s", tr(
                "可像 ShaderToggler 一样添加多个状态文字。所有条目共用上面的字体大小、透明度和描边，只单独保存文字、状态来源、快捷键、位置和显示时间。",
                "Add multiple state labels similar to ShaderToggler. All entries share the global font size, opacity and outline above; each entry only stores its text, source, hotkey, position and display duration."));
            if (ImGui::Button(tr("新增 HUD 条目##AddHUD", "Add HUD item##AddHUD")) && g_settings.hud_indicators.size() < 32)
            {
                hud_indicator item;
                item.x = 0.50f;
                item.y = 0.12f + 0.05f * static_cast<float>(g_settings.hud_indicators.size() % 10);
                g_settings.hud_indicators.push_back(item);
                changed = true;
            }

            for (size_t i = 0; i < g_settings.hud_indicators.size(); )
            {
                auto &item = g_settings.hud_indicators[i];
                ImGui::PushID(static_cast<int>(i));
                std::string title = std::string(item.name.data()) + "##HudNode";
                const bool open = ImGui::TreeNodeEx(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                ImGui::SameLine();
                if (ImGui::SmallButton(tr("删除##DeleteHUD", "Delete##DeleteHUD")))
                {
                    if (g_hotkey_capture_id >= 1000 && g_hotkey_capture_id < 1100)
                        cancel_hotkey_capture();
                    g_settings.hud_indicators.erase(g_settings.hud_indicators.begin() + static_cast<std::ptrdiff_t>(i));
                    changed = true;
                    ImGui::PopID();
                    continue;
                }
                if (open)
                {
                    const bool was_enabled = item.enabled;
                    if (ImGui::Checkbox(tr("启用##HUDEnabled", "Enabled##HUDEnabled"), &item.enabled))
                    {
                        if (item.enabled && !was_enabled)
                            item.runtime_initialized = false;
                        changed = true;
                    }
                    changed |= ImGui::InputText(tr("名称##HUDName", "Name##HUDName"), item.name.data(), item.name.size());
                    const char *source_zh[] = { "快捷键切换状态", "ReShade 实际效果状态", "按住快捷键", "快捷键单次提示" };
                    const char *source_en[] = { "Hotkey toggle state", "Actual ReShade effects state", "Hotkey hold", "Hotkey one-shot message" };
                    int source = static_cast<int>(item.source);
                    if (ImGui::Combo(tr("状态来源##HUDSource", "State source##HUDSource"), &source,
                            g_settings.language == ui_language::chinese ? source_zh : source_en, 4))
                    {
                        item.source = static_cast<hud_state_source>(source);
                        item.runtime_initialized = false;
                        changed = true;
                    }

                    if (item.source != hud_state_source::reshade_effects_state)
                        changed |= draw_hotkey_row(runtime, tr("快捷键", "Hotkey"), 1000 + static_cast<int>(i), item.key);
                    else
                        ImGui::TextDisabled("%s", tr("直接读取 ReShade 当前 Effects State，不需要重复绑定 END。", "Reads ReShade's current Effects State directly; no duplicate END binding is required."));

                    if (item.source == hud_state_source::hotkey_pulse)
                    {
                        changed |= ImGui::InputText(tr("触发文字##HUDOn", "Message text##HUDOn"), item.text_on.data(), item.text_on.size());
                        ImGui::TextDisabled("%s", tr("每次按快捷键都会重新开始显示计时。", "Every shortcut press restarts the display timer."));
                    }
                    else
                    {
                        changed |= ImGui::InputText(tr("ON 文字##HUDOn", "ON text##HUDOn"), item.text_on.data(), item.text_on.size());
                        changed |= ImGui::InputText(tr("OFF 文字##HUDOff", "OFF text##HUDOff"), item.text_off.data(), item.text_off.size());
                    }
                    if (item.source == hud_state_source::hotkey_toggle)
                        changed |= ImGui::Checkbox(tr("初始状态为 ON##HUDInitial", "Initial state ON##HUDInitial"), &item.initial_on);
                    changed |= ImGui::SliderFloat(tr("X##HUDX", "X##HUDX"), &item.x, 0.0f, 1.0f, "%.3f");
                    changed |= ImGui::SliderFloat(tr("Y##HUDY", "Y##HUDY"), &item.y, 0.0f, 1.0f, "%.3f");
                    changed |= ImGui::SliderFloat(tr("状态变化后显示秒数（0=常驻）##HUDSeconds", "Seconds after state change (0=persistent)##HUDSeconds"), &item.show_seconds, 0.0f, 10.0f, "%.1f s");
                    if (item.source == hud_state_source::hotkey_pulse)
                        ImGui::TextDisabled("%s", item.runtime_initialized ? tr("状态：等待快捷键触发", "State: waiting for shortcut") : tr("状态：等待初始化", "State: not initialized"));
                    else
                        ImGui::TextDisabled("%s: %s", tr("当前状态", "Current state"), item.runtime_initialized ? (item.runtime_on ? "ON" : "OFF") : tr("等待初始化", "not initialized"));
                    ImGui::TreePop();
                }
                ImGui::PopID();
                ++i;
            }
        }

        if (ImGui::CollapsingHeader(tr("快捷键##Hotkeys", "Hotkeys##Hotkeys"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextWrapped("%s", tr(
                "点击快捷键框后直接按键或组合键，再点“确定”。不需要输入 Windows VK 数字。",
                "Click a shortcut field, press the key/combo directly, then Apply. Windows VK numbers are no longer entered manually."));
            changed |= draw_hotkey_row(runtime, tr("对比开 / 关", "Comparison on/off"), 1, g_settings.hk_toggle_compare);
            if (g_settings.strategy == capture_strategy::exact_snapshot_pair)
            {
                changed |= draw_hotkey_row(runtime, tr("捕获 Before", "Capture Before"), 2, g_settings.hk_capture_before);
                changed |= draw_hotkey_row(runtime, tr("捕获 After", "Capture After"), 3, g_settings.hk_capture_after);
            }
            else
            {
                changed |= draw_hotkey_row(runtime, tr("冻结 / 解冻", "Freeze/unfreeze"), 4, g_settings.hk_toggle_freeze);
            }
            changed |= draw_hotkey_row(runtime, tr("自动扫屏开 / 关", "Auto sweep on/off"), 5, g_settings.hk_toggle_auto);
            changed |= draw_hotkey_row(runtime, tr("向左移动", "Move left"), 6, g_settings.hk_left);
            changed |= draw_hotkey_row(runtime, tr("向右移动", "Move right"), 7, g_settings.hk_right);
            changed |= draw_hotkey_row(runtime, tr("完整 Before", "Full Before"), 8, g_settings.hk_full_before);
            changed |= draw_hotkey_row(runtime, tr("完整 After", "Full After"), 9, g_settings.hk_full_after);
        }

        if (ImGui::CollapsingHeader(tr("DLSS5 / NGX 高级设置##NGX", "DLSS5 / NGX advanced##NGX")))
        {
            ImGui::TextWrapped("%s", tr(
                "这些参数只影响“实时处理链”模式。精确截图对直接捕获实际显示帧，不依赖 NGX Feature 18。",
                "These options only affect Live pipeline mode. Exact snapshot pairs capture the actually presented frames and do not depend on NGX Feature 18."));
            int max_age = static_cast<int>(g_settings.ngx_max_age_ms);
            if (ImGui::SliderInt(tr("Feature 18 快照最大年龄（ms）##MaxAge", "Max Feature 18 snapshot age (ms)##MaxAge"), &max_age, 1, 1000))
            {
                g_settings.ngx_max_age_ms = static_cast<uint32_t>(max_age);
                changed = true;
            }
            int d3d12_state = static_cast<int>(g_settings.d3d12_color_state);
            if (ImGui::InputInt(tr("D3D12 Color 输入状态（位掩码）##D3D12State", "D3D12 Color input state (bitmask)##D3D12State"), &d3d12_state, 0, 0, ImGuiInputTextFlags_CharsHexadecimal))
            {
                g_settings.d3d12_color_state = static_cast<uint32_t>(d3d12_state);
                changed = true;
                sync_ngx_capture_enabled();
            }
            if (ImGui::Checkbox(tr("允许不安全的跨设备 NGX 导入##UnsafeCrossDevice", "Allow unsafe cross-device NGX import##UnsafeCrossDevice"), &g_settings.allow_unsafe_cross_device_ngx))
                changed = true;
        }

        if (ImGui::CollapsingHeader(tr("诊断与日志##Diagnostics", "Diagnostics & logging##Diagnostics")))
        {
            changed |= ImGui::Checkbox(tr("详细日志##VerboseLog", "Verbose logging##VerboseLog"), &g_settings.verbose_logging);
            ImGui::Text("%s: %s", tr("工作流", "Workflow"), strategy_name(g_settings.strategy));
            if (g_settings.strategy == capture_strategy::live_pipeline)
                ImGui::Text("%s: %s", tr("实时 Before 来源", "Live Before source"), capture_mode_name(g_settings.capture));
            const std::string capture_source_display = localized_runtime_text(state->capture_source);
            ImGui::Text("%s: %s", tr("最近捕获来源", "Last capture source"), capture_source_display.c_str());
            if (!state->capture_note.empty())
            {
                const std::string capture_note_display = localized_runtime_text(state->capture_note);
                ImGui::TextWrapped("%s: %s", tr("说明", "Note"), capture_note_display.c_str());
            }
            ImGui::Text("%s: %s | Before %ux%u | After %ux%u", tr("画面对", "Pair"),
                state->pair_valid ? tr("已就绪", "ready") : tr("未就绪", "not ready"),
                state->before_width, state->before_height, state->after_width, state->after_height);
            ImGui::Text("FrameCompare.fx: %s | %s: %s",
                state->composite != 0 ? tr("已就绪", "ready") : tr("缺失 / 未编译", "missing / not compiled"),
                tr("参数上传", "Parameter upload"), state->params.available ? tr("已就绪", "ready") : tr("未初始化", "not initialized"));

            const auto diag = framecompare::ngx::get_diagnostics();
            ImGui::Text("NGX: %s %ux%u generation %llu", api_name(diag.latest_api), diag.latest_width, diag.latest_height,
                static_cast<unsigned long long>(diag.latest_generation));
            ImGui::Text("%s: %llu / %llu / %llu / %llu",
                tr("Feature18 创建/执行/成功捕获/失败", "Feature18 creates/evaluates/captures/failures"),
                static_cast<unsigned long long>(diag.feature18_creates),
                static_cast<unsigned long long>(diag.feature18_evaluates),
                static_cast<unsigned long long>(diag.successful_captures),
                static_cast<unsigned long long>(diag.failed_captures));
            if (!diag.last_error.empty())
                ImGui::TextWrapped("%s: %s", tr("NGX 底层错误", "NGX hook error"), diag.last_error.c_str());
            if (!g_last_status_message.empty())
                ImGui::TextWrapped("%s: %s", tr("最近状态", "Last status"), g_last_status_message.c_str());
            if (!g_last_warning_message.empty())
                ImGui::TextWrapped("%s: %s", tr("最近警告", "Last warning"), g_last_warning_message.c_str());
            if (!g_last_error_message.empty())
                ImGui::TextWrapped("%s: %s", tr("最近错误", "Last error"), g_last_error_message.c_str());
        }

        if (ImGui::CollapsingHeader(tr("配置文件##Config", "Configuration##Config")))
        {
            ImGui::TextWrapped("%s", tr(
                "全部布局、快捷键和自定义 HUD 条目保存在 00-FrameCompare.addon64 同目录的 FrameCompare.ini，可直接复制到其他游戏。",
                "All layout, hotkeys and custom HUD entries are saved in FrameCompare.ini beside 00-FrameCompare.addon64 and can be copied to other games."));
            if (ImGui::Button(tr("保存 FrameCompare.ini##SaveIni", "Save FrameCompare.ini##SaveIni")))
                save_settings();
            ImGui::SameLine();
            if (ImGui::Button(tr("重新读取 FrameCompare.ini##ReloadIni", "Reload FrameCompare.ini##ReloadIni")))
            {
                load_settings();
                state->pair_valid = false;
                state->exact_before_valid = false;
                state->exact_after_valid = false;
                state->pending_snapshot = snapshot_target::none;
                state->logged_pair_ready = false;
                state->warned_incompatible_pair = false;
                sync_ngx_capture_enabled();
                fc_log(reshade::log::level::info,
                    "已重新读取 FrameCompare.ini。",
                    "FrameCompare.ini reloaded.");
            }
        }

        if (changed)
            mark_settings_dirty();
    }

    void unregister_callbacks()
    {
        reshade::unregister_overlay(nullptr, draw_settings);
        reshade::unregister_event<reshade::addon_event::reshade_open_overlay>(on_open_overlay);
        reshade::unregister_event<reshade::addon_event::reshade_overlay>(on_reshade_overlay);
        reshade::unregister_event<reshade::addon_event::reshade_present>(on_reshade_present);
        reshade::unregister_event<reshade::addon_event::reshade_reloaded_effects>(on_reloaded_effects);
        reshade::unregister_event<reshade::addon_event::reshade_finish_effects>(on_finish_effects);
        reshade::unregister_event<reshade::addon_event::reshade_begin_effects>(on_begin_effects);
        reshade::unregister_event<reshade::addon_event::destroy_effect_runtime>(on_destroy_runtime);
        reshade::unregister_event<reshade::addon_event::init_effect_runtime>(on_init_runtime);
    }
}

extern "C" __declspec(dllexport) bool AddonInit(HMODULE addon_module, HMODULE reshade_module)
{
    g_addon_module = addon_module;
    g_reshade_module = reshade_module;

    wchar_t module_path[MAX_PATH] = {};
    if (GetModuleFileNameW(addon_module, module_path, MAX_PATH) != 0)
        g_ini_path = std::filesystem::path(module_path).parent_path() / L"FrameCompare.ini";
    else
        g_ini_path = L"FrameCompare.ini";

    load_settings();

    if (!reshade::register_addon(addon_module, reshade_module))
        return false;

    reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init_runtime);
    reshade::register_event<reshade::addon_event::destroy_effect_runtime>(on_destroy_runtime);
    reshade::register_event<reshade::addon_event::reshade_begin_effects>(on_begin_effects);
    reshade::register_event<reshade::addon_event::reshade_finish_effects>(on_finish_effects);
    reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(on_reloaded_effects);
    reshade::register_event<reshade::addon_event::reshade_present>(on_reshade_present);
    reshade::register_event<reshade::addon_event::reshade_overlay>(on_reshade_overlay);
    reshade::register_event<reshade::addon_event::reshade_open_overlay>(on_open_overlay);
    reshade::register_overlay(nullptr, draw_settings);

    if (!framecompare::ngx::initialize())
        fc_log(reshade::log::level::warning,
            "NGX Hook 初始化失败；仍可使用“ReShade FX 前”等通用捕获模式。",
            "NGX hook initialization failed; generic modes such as Before ReShade FX remain available.");
    sync_ngx_capture_enabled();

    const std::string init_keys = hotkey_description(g_settings.hk_capture_before) + " Before, " +
        hotkey_description(g_settings.hk_capture_after) + " After, " +
        hotkey_description(g_settings.hk_toggle_compare) + " Compare, " +
        hotkey_description(g_settings.hk_toggle_auto) + " AutoSweep";
    fc_log(reshade::log::level::info,
        "v1.3.0 初始化完成。当前快捷键：" + init_keys,
        "v1.3.0 initialized. Current shortcuts: " + init_keys);
    return true;
}

extern "C" __declspec(dllexport) void AddonUninit(HMODULE addon_module, HMODULE reshade_module)
{
    framecompare::ngx::set_capture_enabled(false);
    if (g_settings_dirty)
        save_settings();

    // Stop the background NGX scanner and remove its detours before unregistering ReShade callbacks.
    framecompare::ngx::shutdown();
    unregister_callbacks();
    reshade::unregister_addon(addon_module, reshade_module);

    g_addon_module = nullptr;
    g_reshade_module = nullptr;
}
