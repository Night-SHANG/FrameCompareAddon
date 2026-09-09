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
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
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
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace reshade::api;
using clock_type = std::chrono::steady_clock;

extern "C" __declspec(dllexport) const char *NAME = "FrameCompare";
extern "C" __declspec(dllexport) const char *AUTHOR = "Night / OpenAI-assisted";
extern "C" __declspec(dllexport) const char *DESCRIPTION =
    "ReShade add-on for plugin-level Before/After comparison, freeze, animated divider, labels and portable profiles.";

namespace
{
    enum class capture_mode : int
    {
        automatic = 0,
        ngx_feature18 = 1,
        before_reshade_fx = 2,
        application_present = 3,
    };

    enum class display_mode : int
    {
        normal_wipe = 0,
        split_screen_cr = 1,
    };

    struct settings
    {
        bool enabled = true;
        capture_mode capture = capture_mode::automatic;
        bool before_on_left = true;
        display_mode display = display_mode::normal_wipe;

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
        std::array<char, 128> font_name { 'S','e','g','o','e',' ','U','I','\0' };
        float before_x = 0.04f;
        float before_y = 0.06f;
        float after_x = 0.88f;
        float after_y = 0.06f;
        int font_size_px = 42;
        float label_opacity = 1.0f;
        float outline_px = 1.5f;

        int hk_toggle_compare = VK_F9;
        int hk_toggle_freeze = VK_F10;
        int hk_toggle_auto = VK_F11;
        int hk_left = VK_LEFT;
        int hk_right = VK_RIGHT;
        int hk_full_before = VK_HOME;
        int hk_full_after = VK_END;

        uint32_t ngx_max_age_ms = 250;
        uint32_t d3d12_color_state = static_cast<uint32_t>(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        bool allow_unsafe_cross_device_ngx = false;
    };

    settings g_settings;
    bool g_frozen = false;
    bool g_freeze_armed = false;
    bool g_auto_sweep_active = false;
    int g_auto_direction = 1;

    HMODULE g_addon_module = nullptr;
    HMODULE g_reshade_module = nullptr;
    std::filesystem::path g_ini_path;
    bool g_settings_dirty = false;
    clock_type::time_point g_last_ini_save = clock_type::now();

    std::mutex g_runtime_mutex;
    std::unordered_map<uint64_t, effect_runtime *> g_runtime_by_swapchain;
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

    void load_settings()
    {
        const auto ini = load_flat_ini(g_ini_path);
        const auto get = [&](const char *key) -> std::string {
            const auto it = ini.find(key);
            return it == ini.end() ? std::string() : it->second;
        };
        const auto geti = [&](const char *key, int fallback) {
            try { const auto v = get(key); return v.empty() ? fallback : std::stoi(v, nullptr, 0); }
            catch (...) { return fallback; }
        };
        const auto getu = [&](const char *key, uint32_t fallback) {
            try { const auto v = get(key); return v.empty() ? fallback : static_cast<uint32_t>(std::stoul(v, nullptr, 0)); }
            catch (...) { return fallback; }
        };
        const auto getf = [&](const char *key, float fallback) {
            try { const auto v = get(key); return v.empty() ? fallback : std::stof(v); }
            catch (...) { return fallback; }
        };
        const auto getb = [&](const char *key, bool fallback) {
            const auto v = get(key);
            return v.empty() ? fallback : parse_bool(v, fallback);
        };

        g_settings.enabled = getb("General.Enabled", g_settings.enabled);
        g_settings.capture = static_cast<capture_mode>(std::clamp(geti("General.CaptureMode", static_cast<int>(g_settings.capture)), 0, 3));
        g_settings.before_on_left = getb("General.BeforeOnLeft", g_settings.before_on_left);
        g_settings.display = static_cast<display_mode>(std::clamp(geti("General.DisplayMode", static_cast<int>(g_settings.display)), 0, 1));

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
        if (const auto v = get("Labels.FontName"); !v.empty()) copy_cstr(g_settings.font_name, v);
        g_settings.before_x = std::clamp(getf("Labels.BeforeX", g_settings.before_x), 0.0f, 1.0f);
        g_settings.before_y = std::clamp(getf("Labels.BeforeY", g_settings.before_y), 0.0f, 1.0f);
        g_settings.after_x = std::clamp(getf("Labels.AfterX", g_settings.after_x), 0.0f, 1.0f);
        g_settings.after_y = std::clamp(getf("Labels.AfterY", g_settings.after_y), 0.0f, 1.0f);
        g_settings.font_size_px = std::clamp(geti("Labels.FontSizePx", g_settings.font_size_px), 8, 256);
        g_settings.label_opacity = std::clamp(getf("Labels.Opacity", g_settings.label_opacity), 0.0f, 1.0f);
        g_settings.outline_px = std::clamp(getf("Labels.OutlinePx", g_settings.outline_px), 0.0f, 12.0f);

        g_settings.hk_toggle_compare = geti("Hotkeys.ToggleCompare", g_settings.hk_toggle_compare);
        g_settings.hk_toggle_freeze = geti("Hotkeys.ToggleFreeze", g_settings.hk_toggle_freeze);
        g_settings.hk_toggle_auto = geti("Hotkeys.ToggleAutoSweep", g_settings.hk_toggle_auto);
        g_settings.hk_left = geti("Hotkeys.MoveLeft", g_settings.hk_left);
        g_settings.hk_right = geti("Hotkeys.MoveRight", g_settings.hk_right);
        g_settings.hk_full_before = geti("Hotkeys.FullBefore", g_settings.hk_full_before);
        g_settings.hk_full_after = geti("Hotkeys.FullAfter", g_settings.hk_full_after);

        g_settings.ngx_max_age_ms = std::clamp(getu("NGX.MaxSnapshotAgeMs", g_settings.ngx_max_age_ms), 1u, 5000u);
        g_settings.d3d12_color_state = getu("NGX.D3D12ColorState", g_settings.d3d12_color_state);
        g_settings.allow_unsafe_cross_device_ngx = getb("NGX.AllowUnsafeCrossDevice", g_settings.allow_unsafe_cross_device_ngx);

        // Transient recording state always starts clean even when a portable INI is reused.
        g_frozen = false;
        g_freeze_armed = false;
        g_auto_sweep_active = false;
        g_auto_direction = g_settings.auto_sweep_direction;
    }

    void save_settings()
    {
        std::ofstream f(g_ini_path, std::ios::binary | std::ios::trunc);
        if (!f)
        {
            reshade::log::message(reshade::log::level::warning, "FrameCompare: unable to write FrameCompare.ini.");
            return;
        }

        f << "; FrameCompare portable configuration (UTF-8)\n";
        f << "; Copy this INI together with 00-FrameCompare.addon64 to reuse the same controls and layout.\n\n";
        f << "[General]\n";
        f << "Enabled=" << (g_settings.enabled ? 1 : 0) << "\n";
        f << "CaptureMode=" << static_cast<int>(g_settings.capture) << "\n";
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
        f << "FontName=" << g_settings.font_name.data() << "\n";
        f << "BeforeX=" << g_settings.before_x << "\n";
        f << "BeforeY=" << g_settings.before_y << "\n";
        f << "AfterX=" << g_settings.after_x << "\n";
        f << "AfterY=" << g_settings.after_y << "\n";
        f << "FontSizePx=" << g_settings.font_size_px << "\n";
        f << "Opacity=" << g_settings.label_opacity << "\n";
        f << "OutlinePx=" << g_settings.outline_px << "\n\n";

        f << "[Hotkeys]\n";
        f << "ToggleCompare=" << g_settings.hk_toggle_compare << "\n";
        f << "ToggleFreeze=" << g_settings.hk_toggle_freeze << "\n";
        f << "ToggleAutoSweep=" << g_settings.hk_toggle_auto << "\n";
        f << "MoveLeft=" << g_settings.hk_left << "\n";
        f << "MoveRight=" << g_settings.hk_right << "\n";
        f << "FullBefore=" << g_settings.hk_full_before << "\n";
        f << "FullAfter=" << g_settings.hk_full_after << "\n\n";

        f << "[NGX]\n";
        f << "MaxSnapshotAgeMs=" << g_settings.ngx_max_age_ms << "\n";
        f << "D3D12ColorState=0x" << std::hex << std::uppercase << g_settings.d3d12_color_state << std::dec << "\n";
        f << "AllowUnsafeCrossDevice=" << (g_settings.allow_unsafe_cross_device_ngx ? 1 : 0) << "\n";

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
        const bool wants_ngx = g_settings.capture == capture_mode::automatic || g_settings.capture == capture_mode::ngx_feature18;
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

    struct label_texture
    {
        resource tex = {};
        resource_view srv = {};
        uint32_t width = 1;
        uint32_t height = 1;
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
        label_texture before_label;
        label_texture after_label;
        parameter_texture params;

        effect_technique composite = {};
        bool warned_missing_fx = false;
        bool warned_param_upload = false;
        bool labels_dirty = true;
        bool pair_valid = false;
        bool fallback_before_this_cycle = false;
        bool before_this_cycle = false;
        bool overlay_open = false;
        bool divider_dragging = false;

        uint64_t effect_cycle = 0;
        uint64_t last_ngx_generation = 0;
        uint64_t present_capture_generation = 0;
        uint64_t last_present_used = 0;
        clock_type::time_point last_tick = clock_type::now();

        move_key_state left_key;
        move_key_state right_key;

        std::string capture_source = "None";
        std::string capture_note;
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

    void destroy_label_texture(device *dev, label_texture &t)
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

    bool ensure_capture_texture(effect_runtime *runtime, gpu_texture &target, resource source, const char *debug_name)
    {
        device *dev = runtime->get_device();
        const resource_desc src_desc = dev->get_resource_desc(source);
        if ((src_desc.type != resource_type::texture_2d && src_desc.type != resource_type::surface) || src_desc.texture.samples != 1)
            return false;

        if (compatible_capture_desc(target, src_desc))
            return true;

        runtime->get_command_queue()->wait_idle();
        destroy_gpu_texture(dev, target);

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

    std::wstring utf8_to_wide(const char *text)
    {
        if (text == nullptr || *text == '\0')
            return L" ";
        const int input_len = static_cast<int>(std::strlen(text));
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, input_len, nullptr, 0);
        if (count <= 0)
            return L" ";
        std::wstring out(static_cast<size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, input_len, out.data(), count);
        return out;
    }

    std::vector<uint8_t> rasterize_text(const char *utf8, const char *font_utf8, int font_px, uint32_t &out_w, uint32_t &out_h)
    {
        const std::wstring text = utf8_to_wide(utf8);
        const std::wstring font_name = utf8_to_wide(font_utf8);
        HDC dc = CreateCompatibleDC(nullptr);
        if (!dc)
        {
            out_w = out_h = 1;
            return std::vector<uint8_t>(4, 0);
        }

        HFONT font = CreateFontW(-font_px, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, font_name.c_str());
        if (!font)
            font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HGDIOBJ old_font = SelectObject(dc, font);

        SIZE size = {};
        GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
        out_w = static_cast<uint32_t>(std::max<LONG>(8, size.cx + 20));
        out_h = static_cast<uint32_t>(std::max<LONG>(8, size.cy + 14));

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = static_cast<LONG>(out_w);
        bmi.bmiHeader.biHeight = -static_cast<LONG>(out_h);
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void *bits = nullptr;
        HBITMAP bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bmp || !bits)
        {
            if (bmp) DeleteObject(bmp);
            SelectObject(dc, old_font);
            if (font && font != GetStockObject(DEFAULT_GUI_FONT)) DeleteObject(font);
            DeleteDC(dc);
            out_w = out_h = 1;
            return std::vector<uint8_t>(4, 0);
        }

        HGDIOBJ old_bmp = SelectObject(dc, bmp);
        std::memset(bits, 0, static_cast<size_t>(out_w) * out_h * 4);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        RECT r { 10, 5, static_cast<LONG>(out_w - 4), static_cast<LONG>(out_h - 2) };
        DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &r, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        std::vector<uint8_t> rgba(static_cast<size_t>(out_w) * out_h * 4);
        const uint8_t *src = static_cast<const uint8_t *>(bits);
        for (size_t i = 0, n = static_cast<size_t>(out_w) * out_h; i < n; ++i)
        {
            const uint8_t coverage = std::max({ src[i * 4 + 0], src[i * 4 + 1], src[i * 4 + 2] });
            rgba[i * 4 + 0] = 255;
            rgba[i * 4 + 1] = 255;
            rgba[i * 4 + 2] = 255;
            rgba[i * 4 + 3] = coverage;
        }

        SelectObject(dc, old_bmp);
        SelectObject(dc, old_font);
        DeleteObject(bmp);
        if (font && font != GetStockObject(DEFAULT_GUI_FONT))
            DeleteObject(font);
        DeleteDC(dc);
        return rgba;
    }

    bool create_label_texture(effect_runtime *runtime, label_texture &target, const char *text)
    {
        uint32_t width = 1, height = 1;
        const auto pixels = rasterize_text(text, g_settings.font_name.data(), g_settings.font_size_px, width, height);
        subresource_data initial = {};
        initial.data = const_cast<uint8_t *>(pixels.data());
        initial.row_pitch = static_cast<uint64_t>(width) * 4;
        initial.slice_pitch = initial.row_pitch * height;

        device *dev = runtime->get_device();
        if (!dev->create_resource(
                resource_desc(width, height, 1, 1, format::r8g8b8a8_unorm, 1, memory_heap::default_, resource_usage::shader_resource),
                &initial, resource_usage::shader_resource, &target.tex))
            return false;
        if (!dev->create_resource_view(target.tex, resource_usage::shader_resource, resource_view_desc(format::r8g8b8a8_unorm), &target.srv))
        {
            dev->destroy_resource(target.tex);
            target = {};
            return false;
        }
        target.width = width;
        target.height = height;
        return true;
    }

    void rebuild_labels(effect_runtime *runtime, runtime_state *state)
    {
        runtime->get_command_queue()->wait_idle();
        device *dev = runtime->get_device();
        destroy_label_texture(dev, state->before_label);
        destroy_label_texture(dev, state->after_label);

        const bool a = create_label_texture(runtime, state->before_label, g_settings.before_text.data());
        const bool b = create_label_texture(runtime, state->after_label, g_settings.after_text.data());
        if (!a || !b)
            reshade::log::message(reshade::log::level::warning, "FrameCompare: failed to rasterize one or more label textures.");

        if (state->before_label.srv != 0)
            runtime->update_texture_bindings("FRAMECOMPARE_LABEL_BEFORE", state->before_label.srv, state->before_label.srv);
        if (state->after_label.srv != 0)
            runtime->update_texture_bindings("FRAMECOMPARE_LABEL_AFTER", state->after_label.srv, state->after_label.srv);
        state->labels_dirty = false;
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
        if (!ensure_parameter_texture(runtime, state))
            return false;

        uint32_t screen_w = 1, screen_h = 1;
        runtime->get_screenshot_width_and_height(&screen_w, &screen_h);
        screen_w = std::max(screen_w, 1u);
        screen_h = std::max(screen_h, 1u);

        const float before_w = static_cast<float>(state->before_label.width) / static_cast<float>(screen_w);
        const float before_h = static_cast<float>(state->before_label.height) / static_cast<float>(screen_h);
        const float after_w = static_cast<float>(state->after_label.width) / static_cast<float>(screen_w);
        const float after_h = static_cast<float>(state->after_label.height) / static_cast<float>(screen_h);

        std::array<float, 32> p = {};
        // texel 0: divider
        p[0] = g_settings.split_position;
        p[1] = g_settings.border_width;
        p[2] = g_settings.border_opacity;
        p[3] = g_settings.show_border ? 1.0f : 0.0f;
        // texel 1: global flags
        p[4] = g_settings.before_on_left ? 1.0f : 0.0f;
        // Label placement preview deliberately forces labels visible even when the
        // normal Show switch is off, so layout can be tuned independently.
        p[5] = (g_settings.show_labels || g_settings.label_preview) ? 1.0f : 0.0f;
        p[6] = g_settings.label_opacity;
        p[7] = g_settings.outline_px;
        // texel 2: before label rect
        p[8] = g_settings.before_x;
        p[9] = g_settings.before_y;
        p[10] = before_w;
        p[11] = before_h;
        // texel 3: after label rect
        p[12] = g_settings.after_x;
        p[13] = g_settings.after_y;
        p[14] = after_w;
        p[15] = after_h;
        // texel 4: label texel size
        p[16] = 1.0f / static_cast<float>(std::max(state->before_label.width, 1u));
        p[17] = 1.0f / static_cast<float>(std::max(state->before_label.height, 1u));
        p[18] = 1.0f / static_cast<float>(std::max(state->after_label.width, 1u));
        p[19] = 1.0f / static_cast<float>(std::max(state->after_label.height, 1u));
        // texel 5: display/preview flags
        p[20] = static_cast<float>(static_cast<int>(g_settings.display));
        p[21] = g_settings.label_preview ? 1.0f : 0.0f;
        p[22] = (g_settings.enabled && state->pair_valid && state->before.ready && state->after.ready) ? 1.0f : 0.0f;
        // p[23] and texels 6/7 are reserved for forward compatibility.

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
        state->composite = runtime->find_technique("FrameCompare.fx", "FrameCompareComposite");
        if (state->composite != 0)
            runtime->set_technique_state(state->composite, false);
        state->warned_missing_fx = false;
        if (state->params.srv != 0)
            runtime->update_texture_bindings("FRAMECOMPARE_PARAMS", state->params.srv, state->params.srv);
        if (state->before.srv != 0)
            runtime->update_texture_bindings("FRAMECOMPARE_BEFORE", state->before.srv, state->before.srv);
        if (state->after.srv != 0)
            runtime->update_texture_bindings("FRAMECOMPARE_AFTER", state->after.srv, state->after.srv);
        if (state->before_label.srv != 0)
            runtime->update_texture_bindings("FRAMECOMPARE_LABEL_BEFORE", state->before_label.srv, state->before_label.srv);
        if (state->after_label.srv != 0)
            runtime->update_texture_bindings("FRAMECOMPARE_LABEL_AFTER", state->after_label.srv, state->after_label.srv);
    }

    bool is_primary_runtime(effect_runtime *runtime)
    {
        std::lock_guard lock(g_runtime_mutex);
        if (g_primary_runtime == nullptr)
            g_primary_runtime = runtime;
        return g_primary_runtime == runtime;
    }

    void set_freeze_requested(runtime_state *state, bool freeze)
    {
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

    void process_move_key(effect_runtime *runtime, move_key_state &state, uint32_t vk, int direction, float dt, clock_type::time_point now)
    {
        const bool down = runtime->is_key_down(vk);
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

    void update_hotkeys(effect_runtime *runtime, runtime_state *state)
    {
        if (!is_primary_runtime(runtime))
            return;

        const auto now = clock_type::now();
        float dt = std::chrono::duration<float>(now - state->last_tick).count();
        state->last_tick = now;
        dt = std::clamp(dt, 0.0f, 0.10f);

        if (runtime->is_key_pressed(g_settings.hk_toggle_compare))
        {
            g_settings.enabled = !g_settings.enabled;
            // A preview-only frame may have reused the After capture as its neutral
            // background. Never revive an old/mixed pair when comparison is toggled.
            state->pair_valid = false;
            state->before_this_cycle = false;
            state->fallback_before_this_cycle = false;
            if (!g_settings.enabled)
            {
                g_auto_sweep_active = false;
                g_frozen = false;
                g_freeze_armed = false;
            }
            mark_settings_dirty();
            sync_ngx_capture_enabled();
        }
        if (runtime->is_key_pressed(g_settings.hk_toggle_freeze))
            set_freeze_requested(state, !(g_frozen || g_freeze_armed));
        if (runtime->is_key_pressed(g_settings.hk_toggle_auto))
            toggle_auto_sweep();
        if (runtime->is_key_pressed(g_settings.hk_full_before))
        {
            g_auto_sweep_active = false;
            g_settings.split_position = full_before_position();
            mark_settings_dirty();
        }
        if (runtime->is_key_pressed(g_settings.hk_full_after))
        {
            g_auto_sweep_active = false;
            g_settings.split_position = full_after_position();
            mark_settings_dirty();
        }

        process_move_key(runtime, state->left_key, static_cast<uint32_t>(g_settings.hk_left), -1, dt, now);
        process_move_key(runtime, state->right_key, static_cast<uint32_t>(g_settings.hk_right), 1, dt, now);
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
        if (state->labels_dirty)
            rebuild_labels(runtime, state);
        if (state->composite == 0)
        {
            refresh_effect_handles(runtime, state);
            if (state->composite == 0)
            {
                if (!state->warned_missing_fx)
                {
                    reshade::log::message(reshade::log::level::warning,
                        "FrameCompare: FrameCompare.fx / FrameCompareComposite was not found. Install the companion shader in ReShade's shader search path.");
                    state->warned_missing_fx = true;
                }
                return false;
            }
        }

        if (!update_parameter_texture(runtime, state, cmd))
        {
            if (!state->warned_param_upload)
            {
                reshade::log::message(reshade::log::level::error,
                    "FrameCompare: this graphics backend does not support command-list texture uploads required by the Performance-Mode-safe parameter path.");
                state->warned_param_upload = true;
            }
            return false;
        }

        // Preview-only mode may run before a valid Before/After pair exists. In that
        // case bind the current post-effects snapshot to both image semantics; the
        // shader uses it as a neutral full-screen base while both labels are shown.
        const resource_view preview_base = state->after.srv;
        const resource_view before_srv = state->before.ready ? state->before.srv : preview_base;
        const resource_view after_srv = state->after.ready ? state->after.srv : before_srv;
        if (before_srv == 0 || after_srv == 0)
            return false;

        runtime->update_texture_bindings("FRAMECOMPARE_BEFORE", before_srv, before_srv);
        runtime->update_texture_bindings("FRAMECOMPARE_AFTER", after_srv, after_srv);
        runtime->update_texture_bindings("FRAMECOMPARE_PARAMS", state->params.srv, state->params.srv);
        if (state->before_label.srv != 0)
            runtime->update_texture_bindings("FRAMECOMPARE_LABEL_BEFORE", state->before_label.srv, state->before_label.srv);
        if (state->after_label.srv != 0)
            runtime->update_texture_bindings("FRAMECOMPARE_LABEL_AFTER", state->after_label.srv, state->after_label.srv);
        return true;
    }

    void on_init_runtime(effect_runtime *runtime)
    {
        auto *state = runtime->create_private_data<runtime_state>();
        state->last_tick = clock_type::now();
        {
            std::lock_guard lock(g_runtime_mutex);
            g_runtime_by_swapchain[runtime->get_native()] = runtime;
            if (g_primary_runtime == nullptr)
                g_primary_runtime = runtime;
        }
        refresh_effect_handles(runtime, state);
    }

    void on_destroy_runtime(effect_runtime *runtime)
    {
        {
            std::lock_guard lock(g_runtime_mutex);
            g_runtime_by_swapchain.erase(runtime->get_native());
            if (g_primary_runtime == runtime)
                g_primary_runtime = nullptr;
        }

        if (auto *state = runtime->get_private_data<runtime_state>())
        {
            runtime->get_command_queue()->wait_idle();
            device *dev = runtime->get_device();
            destroy_gpu_texture(dev, state->before);
            destroy_gpu_texture(dev, state->after);
            destroy_label_texture(dev, state->before_label);
            destroy_label_texture(dev, state->after_label);
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
            refresh_effect_handles(runtime, state);
    }

    void on_present(command_queue *queue, swapchain *swapchain, const rect *, const rect *, uint32_t, const rect *)
    {
        if (!g_settings.enabled || g_frozen || g_settings.capture != capture_mode::application_present)
            return;

        effect_runtime *runtime = nullptr;
        {
            std::lock_guard lock(g_runtime_mutex);
            const auto it = g_runtime_by_swapchain.find(swapchain->get_native());
            if (it != g_runtime_by_swapchain.end())
                runtime = it->second;
        }
        if (!runtime)
            return;
        auto *state = runtime->get_private_data<runtime_state>();
        if (!state)
            return;

        const resource backbuffer = runtime->get_current_back_buffer();
        command_list *cmd = queue->get_immediate_command_list();
        if (copy_into_capture(runtime, cmd, backbuffer, resource_usage::present, state->before, "FrameCompare Before (Present)"))
        {
            ++state->present_capture_generation;
            state->before_width = state->before.width;
            state->before_height = state->before.height;
            state->capture_source = "Application Present hook backbuffer (ordering-dependent generic mode)";
            state->capture_note.clear();
        }
    }

    void on_begin_effects(effect_runtime *runtime, command_list *cmd, resource_view rtv, resource_view)
    {
        if (!g_settings.enabled || g_frozen)
            return;
        auto *state = runtime->get_private_data<runtime_state>();
        if (!state)
            return;

        ++state->effect_cycle;
        state->fallback_before_this_cycle = false;
        state->before_this_cycle = false;

        if (state->composite != 0)
            runtime->set_technique_state(state->composite, false);

        // Auto mode intentionally keeps a guaranteed same-frame generic fallback. If a fresh
        // Feature 18 snapshot exists by finish_effects, it overwrites this fallback before compositing.
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
        else if (g_settings.capture == capture_mode::application_present &&
                 state->present_capture_generation != state->last_present_used)
        {
            state->before_this_cycle = state->before.ready;
            state->last_present_used = state->present_capture_generation;
        }
    }

    void on_finish_effects(effect_runtime *runtime, command_list *cmd, resource_view rtv, resource_view rtv_srgb)
    {
        const bool compare_enabled = g_settings.enabled;
        const bool preview_enabled = g_settings.label_preview;
        if (!compare_enabled && !preview_enabled)
            return;

        auto *state = runtime->get_private_data<runtime_state>();
        if (!state)
            return;

        if (compare_enabled && !g_frozen)
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

            if (g_settings.capture == capture_mode::application_present && !state->before_this_cycle)
            {
                state->capture_note = "No fresh Present-stage Before image was available for this ReShade effect cycle.";
            }

            if (state->before_this_cycle)
            {
                const resource target = runtime->get_device()->get_resource_from_view(rtv);
                if (copy_into_capture(runtime, cmd, target, resource_usage::render_target, state->after, "FrameCompare After"))
                {
                    state->after_width = state->after.width;
                    state->after_height = state->after.height;
                    state->pair_valid = state->before.ready && state->after.ready;
                    if (g_freeze_armed && state->pair_valid)
                    {
                        g_freeze_armed = false;
                        g_frozen = true;
                        sync_ngx_capture_enabled();
                    }
                }
            }
        }

        const bool use_compare_pair = compare_enabled && state->pair_valid && state->before.ready && state->after.ready;

        // Label placement preview is intentionally independent of the comparison
        // capture. If no valid pair is being displayed, save the current final
        // ReShade target as a neutral base so both labels can still be positioned
        // live in-game. This copy exists only while preview mode is enabled.
        if (preview_enabled && !use_compare_pair)
        {
            const resource target = runtime->get_device()->get_resource_from_view(rtv);
            if (!copy_into_capture(runtime, cmd, target, resource_usage::render_target, state->after, "FrameCompare Label Preview Base"))
                return;
            state->after_width = state->after.width;
            state->after_height = state->after.height;
        }

        if (!use_compare_pair && !preview_enabled)
            return;
        if (!prepare_compositor(runtime, state, cmd))
            return;

        runtime->render_technique(state->composite, cmd, rtv, rtv_srgb);
    }

    void on_reshade_present(effect_runtime *runtime)
    {
        if (auto *state = runtime->get_private_data<runtime_state>())
            update_hotkeys(runtime, state);
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
        case capture_mode::automatic: return "Auto: NGX Feature 18, fallback to Before ReShade FX";
        case capture_mode::ngx_feature18: return "NGX Feature 18 strict (pre-Evaluate Color)";
        case capture_mode::before_reshade_fx: return "Before ReShade FX (recommended for Feeder/effect-chain injection)";
        case capture_mode::application_present: return "Application Present hook (generic / ordering dependent)";
        }
        return "Unknown";
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

    void draw_settings(effect_runtime *runtime)
    {
        auto *state = runtime->get_private_data<runtime_state>();
        if (!state)
            return;

        bool changed = false;
        bool labels_changed = false;

        if (ImGui::Checkbox("Enable comparison", &g_settings.enabled))
        {
            changed = true;
            state->pair_valid = false;
            state->before_this_cycle = false;
            state->fallback_before_this_cycle = false;
            if (!g_settings.enabled)
            {
                g_frozen = false;
                g_freeze_armed = false;
                g_auto_sweep_active = false;
            }
            sync_ngx_capture_enabled();
        }

        bool freeze_ui = g_frozen || g_freeze_armed;
        if (ImGui::Checkbox("Freeze Before + After pair", &freeze_ui))
            set_freeze_requested(state, freeze_ui);
        if (g_freeze_armed)
            ImGui::TextDisabled("Freeze is armed and will lock on the next valid Before/After pair.");

        int capture = static_cast<int>(g_settings.capture);
        const char *capture_items[] = {
            "Auto: NGX Feature 18 -> Before ReShade FX fallback",
            "NGX Feature 18 strict",
            "Before ReShade FX",
            "Application Present hook"
        };
        if (ImGui::Combo("Before capture mode", &capture, capture_items, IM_ARRAYSIZE(capture_items)))
        {
            g_settings.capture = static_cast<capture_mode>(capture);
            changed = true;
            state->pair_valid = false;
            sync_ngx_capture_enabled();
        }
        ImGui::TextWrapped("For RenoDX/native Neural Rendering, try Auto first. For DLSS5 Feeder or another effect that runs inside the ReShade effect chain, use Before ReShade FX. Strict NGX never falls back to a potentially different capture stage.");

        if (ImGui::Checkbox("Before image on left", &g_settings.before_on_left)) changed = true;
        int display = static_cast<int>(g_settings.display);
        const char *display_items[] = { "Normal wipe (same full-frame coordinates)", "SplitScreenCR-style centered remap" };
        if (ImGui::Combo("Display mode", &display, display_items, IM_ARRAYSIZE(display_items)))
        {
            g_settings.display = static_cast<display_mode>(display);
            changed = true;
        }

        if (ImGui::SliderFloat("Divider position", &g_settings.split_position, 0.0f, 1.0f, "%.3f"))
        {
            g_auto_sweep_active = false;
            changed = true;
        }
        changed |= ImGui::SliderFloat("Short press step", &g_settings.move_step, 0.001f, 0.20f, "%.3f");
        changed |= ImGui::SliderFloat("Hold delay (s)", &g_settings.hold_delay, 0.0f, 1.0f, "%.2f");
        changed |= ImGui::SliderFloat("Hold movement speed (screen/s)", &g_settings.move_speed, 0.01f, 2.0f, "%.2f");

        if (ImGui::Checkbox("Auto sweep active", &g_auto_sweep_active))
        {
            if (g_auto_sweep_active)
                start_auto_sweep();
        }
        changed |= ImGui::SliderFloat("Auto sweep speed", &g_settings.auto_sweep_speed, 0.01f, 2.0f, "%.2f");
        changed |= ImGui::Checkbox("Auto sweep ping-pong", &g_settings.auto_sweep_pingpong);
        changed |= ImGui::Checkbox("Auto sweep starts from full Before", &g_settings.auto_reset_from_before);
        if (!g_settings.auto_reset_from_before)
        {
            int dir = g_settings.auto_sweep_direction;
            if (ImGui::RadioButton("Physical direction: left -> right", dir > 0)) { g_settings.auto_sweep_direction = 1; changed = true; }
            ImGui::SameLine();
            if (ImGui::RadioButton("right -> left", dir < 0)) { g_settings.auto_sweep_direction = -1; changed = true; }
        }

        changed |= ImGui::Checkbox("Show divider", &g_settings.show_border);
        changed |= ImGui::SliderFloat("Divider width", &g_settings.border_width, 0.0f, 0.02f, "%.4f");
        changed |= ImGui::SliderFloat("Divider opacity", &g_settings.border_opacity, 0.0f, 1.0f, "%.2f");
        changed |= ImGui::Checkbox("Drag divider while ReShade overlay is open", &g_settings.screen_drag);
        changed |= ImGui::SliderFloat("Drag grab radius (px)", &g_settings.drag_grab_px, 2.0f, 60.0f, "%.0f");

        ImGui::SeparatorText("Labels");
        changed |= ImGui::Checkbox("Show labels", &g_settings.show_labels);
        changed |= ImGui::Checkbox("Label placement preview (always show both)", &g_settings.label_preview);
        ImGui::TextWrapped("Preview mode keeps both labels visible while you tune X/Y, size and outline. It works even before a valid Before/After pair exists; when comparison is unavailable it draws over the current post-effects frame. Disable preview before normal recording.");
        if (ImGui::InputText("Before label", g_settings.before_text.data(), g_settings.before_text.size())) { labels_changed = changed = true; }
        if (ImGui::InputText("After label", g_settings.after_text.data(), g_settings.after_text.size())) { labels_changed = changed = true; }
        if (ImGui::InputText("Windows font", g_settings.font_name.data(), g_settings.font_name.size())) { labels_changed = changed = true; }
        changed |= ImGui::SliderFloat("Before label X", &g_settings.before_x, 0.0f, 1.0f, "%.3f");
        changed |= ImGui::SliderFloat("Before label Y", &g_settings.before_y, 0.0f, 1.0f, "%.3f");
        changed |= ImGui::SliderFloat("After label X", &g_settings.after_x, 0.0f, 1.0f, "%.3f");
        changed |= ImGui::SliderFloat("After label Y", &g_settings.after_y, 0.0f, 1.0f, "%.3f");
        if (ImGui::SliderInt("Label font size", &g_settings.font_size_px, 8, 160)) { labels_changed = changed = true; }
        changed |= ImGui::SliderFloat("Label opacity", &g_settings.label_opacity, 0.0f, 1.0f, "%.2f");
        changed |= ImGui::SliderFloat("Label outline", &g_settings.outline_px, 0.0f, 8.0f, "%.1f px");
        ImGui::TextDisabled("For Chinese text, select a Windows font that contains the glyphs, e.g. Microsoft YaHei UI.");

        ImGui::SeparatorText("Hotkeys (Windows virtual-key codes)");
        changed |= ImGui::InputInt("Toggle comparison", &g_settings.hk_toggle_compare);
        changed |= ImGui::InputInt("Toggle freeze", &g_settings.hk_toggle_freeze);
        changed |= ImGui::InputInt("Toggle auto sweep", &g_settings.hk_toggle_auto);
        changed |= ImGui::InputInt("Move divider left", &g_settings.hk_left);
        changed |= ImGui::InputInt("Move divider right", &g_settings.hk_right);
        changed |= ImGui::InputInt("Show full Before", &g_settings.hk_full_before);
        changed |= ImGui::InputInt("Show full After", &g_settings.hk_full_after);
        ImGui::TextDisabled("Defaults: F9=120, F10=121, F11=122, Left=37, Right=39, Home=36, End=35");

        ImGui::SeparatorText("NGX / DLSS5 capture");
        int max_age = static_cast<int>(g_settings.ngx_max_age_ms);
        if (ImGui::SliderInt("Max Feature 18 snapshot age (ms)", &max_age, 1, 1000))
        {
            g_settings.ngx_max_age_ms = static_cast<uint32_t>(max_age);
            changed = true;
        }
        int d3d12_state = static_cast<int>(g_settings.d3d12_color_state);
        if (ImGui::InputInt("D3D12 Color input state (bitmask)", &d3d12_state, 0, 0, ImGuiInputTextFlags_CharsHexadecimal))
        {
            g_settings.d3d12_color_state = static_cast<uint32_t>(d3d12_state);
            changed = true;
            sync_ngx_capture_enabled();
        }
        if (ImGui::Checkbox("Allow unsafe cross-device NGX import", &g_settings.allow_unsafe_cross_device_ngx))
            changed = true;
        ImGui::TextWrapped("Leave cross-device import OFF unless diagnosing a bridge that creates a private D3D12 device. A shared resource handle alone does not provide queue synchronization; enabling this can produce stale/undefined data and is not a correctness guarantee.");

        ImGui::SeparatorText("Diagnostics");
        ImGui::Text("Configured mode: %s", capture_mode_name(g_settings.capture));
        ImGui::Text("Current source: %s", state->capture_source.c_str());
        if (!state->capture_note.empty())
            ImGui::TextWrapped("Capture note: %s", state->capture_note.c_str());
        ImGui::Text("Pair: %s | Before %ux%u | After %ux%u", state->pair_valid ? "ready" : "not ready",
            state->before_width, state->before_height, state->after_width, state->after_height);
        ImGui::Text("Freeze: %s | Sweep: %s | Label preview: %s", g_frozen ? "frozen" : (g_freeze_armed ? "armed" : "live"),
            g_auto_sweep_active ? "active" : "off", g_settings.label_preview ? "on" : "off");
        ImGui::Text("FrameCompare.fx: %s", state->composite != 0 ? "ready" : "missing / not compiled");
        ImGui::Text("Parameter upload: %s", state->params.available ? "ready" : "not initialized");

        const auto diag = framecompare::ngx::get_diagnostics();
        ImGui::Text("NGX hook module: %ls", diag.hook_module.empty() ? L"(none)" : diag.hook_module.c_str());
        ImGui::Text("NGX latest: %s %ux%u generation %llu", api_name(diag.latest_api), diag.latest_width, diag.latest_height,
            static_cast<unsigned long long>(diag.latest_generation));
        ImGui::Text("Feature18 creates/evaluates/captures/failures: %llu / %llu / %llu / %llu",
            static_cast<unsigned long long>(diag.feature18_creates),
            static_cast<unsigned long long>(diag.feature18_evaluates),
            static_cast<unsigned long long>(diag.successful_captures),
            static_cast<unsigned long long>(diag.failed_captures));
        ImGui::Text("Hooks D3D11 C/E/R: %d/%d/%d | D3D12 C/E/R: %d/%d/%d",
            diag.create11_hooked, diag.eval11_hooked || diag.eval11_c_hooked, diag.release11_hooked,
            diag.create12_hooked, diag.eval12_hooked || diag.eval12_c_hooked, diag.release12_hooked);
        if (!diag.last_error.empty())
            ImGui::TextWrapped("NGX hook error: %s", diag.last_error.c_str());

        if (ImGui::Button("Save FrameCompare.ini"))
            save_settings();
        ImGui::SameLine();
        if (ImGui::Button("Reload FrameCompare.ini"))
        {
            load_settings();
            state->labels_dirty = true;
            state->pair_valid = false;
            sync_ngx_capture_enabled();
        }

        if (labels_changed)
            state->labels_dirty = true;
        if (changed)
            mark_settings_dirty();
    }

    void unregister_callbacks()
    {
        reshade::unregister_overlay(nullptr, draw_settings);
        reshade::unregister_event<reshade::addon_event::reshade_open_overlay>(on_open_overlay);
        reshade::unregister_event<reshade::addon_event::reshade_present>(on_reshade_present);
        reshade::unregister_event<reshade::addon_event::reshade_reloaded_effects>(on_reloaded_effects);
        reshade::unregister_event<reshade::addon_event::reshade_finish_effects>(on_finish_effects);
        reshade::unregister_event<reshade::addon_event::reshade_begin_effects>(on_begin_effects);
        reshade::unregister_event<reshade::addon_event::present>(on_present);
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
    reshade::register_event<reshade::addon_event::present>(on_present);
    reshade::register_event<reshade::addon_event::reshade_begin_effects>(on_begin_effects);
    reshade::register_event<reshade::addon_event::reshade_finish_effects>(on_finish_effects);
    reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(on_reloaded_effects);
    reshade::register_event<reshade::addon_event::reshade_present>(on_reshade_present);
    reshade::register_event<reshade::addon_event::reshade_open_overlay>(on_open_overlay);
    reshade::register_overlay(nullptr, draw_settings);

    if (!framecompare::ngx::initialize())
        reshade::log::message(reshade::log::level::warning,
            "FrameCompare: NGX hook initialization failed. Generic Before ReShade FX mode remains available.");
    sync_ngx_capture_enabled();

    reshade::log::message(reshade::log::level::info,
        "FrameCompare: initialized. F9 compare, F10 freeze, F11 auto sweep, arrows move divider, Home/End full Before/After.");
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
