#include "ui/i18n/localization.hpp"

#include <array>

namespace framecompare::ui::i18n
{
namespace
{
using Table = std::array<const char *, static_cast<std::size_t>(TextId::count)>;

UiLanguage g_language = UiLanguage::zh_cn;

constexpr Table chinese = {
    "界面语言", "简体中文", "English",
    "对比", "扫屏", "快捷键", "标签", "状态", "配置",
    "启用实时对比", "分割位置", "Before 位于左侧", "显示方式",
    "同坐标擦除", "中心偏移", "中心取景位置",
    "中心偏移适合固定对比，不推荐配合自动扫屏。",
    "DLSS5 处理前画面作为 Before",
    "勾选后，同坐标模式的 Before 区域使用 DLSSNR.Color；After 仍为现有 Post-ReShade FX。",
    "中心偏移暂不支持安全的 DLSS5 区域回填；当前自动使用原本的 Pre-ReShade FX Before。",
    "DLSS5 诊断", "DLSS5 模块", "等待载入", "已载入", "Evaluate 挂钩", "调用统计",
    "区域复制", "最后结果", "最后错误",
    "显示分割线", "线宽", "线透明度", "恢复分屏默认",
    "冻结当前画面对", "手动移动速度", "自动扫屏速度", "自动模式",
    "左到右", "右到左", "往返", "停止并回中", "开始自动扫屏",
    "恢复移动默认",
    "左移", "右移", "中心取景向左", "中心取景向右", "启用/关闭实时对比",
    "自动开始/停止", "冻结/继续", "切换显示方式", "显示/隐藏分割线",
    "恢复默认快捷键", "未设置", "清除", "请按键...",
    "显示标签", "Before 文字", "After 文字", "左侧 X", "左侧 Y",
    "右侧 X", "右侧 Y", "镜像左侧位置到右侧", "镜像右侧位置到左侧",
    "字号", "文字透明度", "描边宽度", "描边透明度", "恢复标签默认", "重置",
    "ReShade 状态直接读取真实效果开关；快捷键跟踪只根据相同按键翻转，不能验证外部插件的真实状态。",
    "新增状态条目", "移除", "启用", "名称", "状态来源",
    "快捷键跟踪状态", "ReShade 实际效果状态", "快捷键", "初始状态为 ON",
    "直接读取 ReShade 当前状态，无需绑定 END。", "ON 文字", "OFF 文字",
    "X", "Y", "显示秒数（0=常驻）", "当前状态",
    "语言会自动保存到 FrameCompare.ini。",
    "所有设置自动保存到插件同目录的 FrameCompare.ini。", "立即保存", "重新读取",
    "无法写入 FrameCompare.ini", "已保存 FrameCompare.ini",
    "无法读取 FrameCompare.ini", "已重新读取 FrameCompare.ini",
    "捕获：Pre-ReShade FX / Post-ReShade FX（同帧）", "等待同帧画面对",
    "Pre-ReShade FX 未被认证为 Vanilla。"
};

constexpr Table english = {
    "Interface language", "简体中文", "English",
    "Compare", "Motion", "Hotkeys", "Labels", "Status", "Config",
    "Enable realtime comparison", "Split position", "Before on left", "Display mode",
    "Same-coordinate wipe", "Center remap", "Center focus",
    "Center remap is intended for fixed comparisons; autosweep is not recommended.",
    "Use pre-DLSS5 image for Before",
    "When enabled, the same-coordinate Before region uses DLSSNR.Color; After remains the existing Post-ReShade FX result.",
    "Center remap does not yet support safe DLSS5 region replacement; the original Pre-ReShade FX Before is used.",
    "DLSS5 diagnostics", "DLSS5 module", "Waiting", "Loaded", "Evaluate hooks", "Calls",
    "Region copies", "Last result", "Last error",
    "Show border", "Border width", "Border opacity", "Reset comparison defaults",
    "Freeze current pair", "Manual speed", "Auto speed", "Auto mode",
    "Left to right", "Right to left", "Ping-pong", "Stop and center", "Start autosweep",
    "Reset motion defaults",
    "Move left", "Move right", "Center focus left", "Center focus right",
    "Toggle comparison", "Toggle autosweep", "Toggle freeze", "Toggle display mode",
    "Toggle border", "Reset default hotkeys", "Unbound", "Clear", "Press a key...",
    "Show labels", "Before text", "After text", "Left X", "Left Y", "Right X", "Right Y",
    "Mirror left to right", "Mirror right to left", "Font size", "Text opacity",
    "Outline width", "Outline opacity", "Reset label defaults", "Reset",
    "ReShade state reads the real effects switch. Tracked hotkeys only toggle locally and cannot verify another add-on's state.",
    "Add status item", "Remove", "Enabled", "Name", "State source",
    "Tracked hotkey state", "Actual ReShade effects state", "Hotkey", "Initial state ON",
    "Reads the current ReShade state directly; no END binding is needed.",
    "ON text", "OFF text", "X", "Y", "Seconds (0=persistent)", "Current state",
    "The language is saved automatically in FrameCompare.ini.",
    "All settings are saved automatically to FrameCompare.ini beside the add-on.",
    "Save now", "Reload", "Unable to write FrameCompare.ini", "FrameCompare.ini saved",
    "Unable to read FrameCompare.ini", "FrameCompare.ini reloaded",
    "Capture: Pre-ReShade FX / Post-ReShade FX (same frame)",
    "Waiting for a same-frame pair", "Pre-ReShade FX is not verified as Vanilla."
};

static_assert(chinese.size() == english.size());

const Table &table(UiLanguage value) noexcept
{
    return value == UiLanguage::en ? english : chinese;
}
}

UiLanguage language() noexcept
{
    return g_language;
}

void set_language(UiLanguage value) noexcept
{
    g_language = value;
}

UiLanguage parse_language(std::string_view value) noexcept
{
    return value == "en" ? UiLanguage::en : UiLanguage::zh_cn;
}

const char *language_code(UiLanguage value) noexcept
{
    return value == UiLanguage::en ? "en" : "zh-CN";
}

const char *text(TextId id) noexcept
{
    return text(id, g_language);
}

const char *text(TextId id, UiLanguage value) noexcept
{
    const auto index = static_cast<std::size_t>(id);
    return index < table(value).size() ? table(value)[index] : "Invalid TextId";
}

std::string label(TextId id, std::string_view stable_id)
{
    std::string result = text(id);
    result += "##";
    result += stable_id;
    return result;
}
}
