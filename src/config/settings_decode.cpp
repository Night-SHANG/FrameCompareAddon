#include "config/settings_codec.hpp"

#include "config/ini_document.hpp"
#include "control/split_motion.hpp"
#include "hud/indicator_model.hpp"
#include "input/hotkeys.hpp"
#include "render/compositor.hpp"
#include "ui/label_overlay.hpp"
#include "ui/i18n/localization.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

namespace framecompare::config
{
namespace
{
class Reader
{
public:
    explicit Reader(const IniDocument &document) : document_(document) {}

    int integer(const char *section, const char *key, int fallback) const
    {
        try
        {
            const auto value = document_.get(section, key);
            return value ? std::stoi(*value, nullptr, 0) : fallback;
        }
        catch (...)
        {
            return fallback;
        }
    }

    float number(const char *section, const char *key, float fallback) const
    {
        try
        {
            const auto value = document_.get(section, key);
            if (!value)
                return fallback;
            const float parsed = std::stof(*value);
            return std::isfinite(parsed) ? parsed : fallback;
        }
        catch (...)
        {
            return fallback;
        }
    }

    bool boolean(const char *section, const char *key, bool fallback) const
    {
        const auto stored = document_.get(section, key);
        if (!stored)
            return fallback;
        std::string value = *stored;
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
        if (value == "1" || value == "true" || value == "yes" ||
            value == "on")
            return true;
        if (value == "0" || value == "false" || value == "no" ||
            value == "off")
            return false;
        return fallback;
    }

    std::string value(const char *section, const char *key,
                      std::string fallback) const
    {
        return document_.get(section, key).value_or(fallback);
    }

    template <std::size_t Size>
    void text(const char *section, const char *key,
              std::array<char, Size> &destination) const
    {
        const auto stored = document_.get(section, key);
        if (!stored)
            return;
        const std::size_t count = std::min(Size - 1, stored->size());
        std::memcpy(destination.data(), stored->data(), count);
        destination[count] = '\0';
    }

private:
    const IniDocument &document_;
};

ImGuiKeyChord normalize_chord(int value, ImGuiKeyChord fallback)
{
    if (value == 0)
        return ImGuiKey_None;
    const auto chord = static_cast<ImGuiKeyChord>(std::max(value, 0));
    const auto named_key = static_cast<ImGuiKey>(chord & ~ImGuiMod_Mask_);
    if (named_key < ImGuiKey_NamedKey_BEGIN ||
        named_key >= ImGuiKey_NamedKey_END)
        return fallback;
    return named_key | (chord & ImGuiMod_Mask_);
}

ImGuiKeyChord read_chord(const Reader &reader, const char *key,
                         ImGuiKeyChord fallback)
{
    return normalize_chord(reader.integer(
        "Hotkeys", key, static_cast<int>(fallback)), fallback);
}
}

void decode_current_settings(std::string_view text)
{
    const IniDocument document = IniDocument::parse(text);
    const Reader reader(document);

    ui::i18n::set_language(ui::i18n::parse_language(
        reader.value("UI", "Language", "zh-CN")));

    auto &compositor = render::settings();
    compositor = render::CompositorSettings{};
    compositor.enabled = reader.boolean(
        "General", "Enabled", compositor.enabled);
    compositor.before_on_left = reader.boolean(
        "General", "BeforeOnLeft", compositor.before_on_left);
    compositor.display_mode = static_cast<render::DisplayMode>(std::clamp(
        reader.integer("General", "DisplayMode",
                       static_cast<int>(compositor.display_mode)), 0, 1));
    compositor.split_position = std::clamp(reader.number(
        "Divider", "Position", compositor.split_position), 0.0f, 1.0f);
    compositor.center_focus = std::clamp(reader.number(
        "Divider", "CenterFocus", compositor.center_focus), 0.0f, 1.0f);
    compositor.show_border = reader.boolean(
        "Divider", "ShowBorder", compositor.show_border);
    compositor.border_width = std::clamp(reader.number(
        "Divider", "BorderWidth", compositor.border_width), 0.0f, 0.02f);
    compositor.border_opacity = std::clamp(reader.number(
        "Divider", "BorderOpacity", compositor.border_opacity), 0.0f, 1.0f);

    auto &motion = control::split_motion().settings();
    motion = control::MotionSettings{};
    motion.manual_speed = std::clamp(reader.number(
        "Motion", "ManualSpeed", motion.manual_speed), 0.01f, 1.0f);
    motion.auto_speed = std::clamp(reader.number(
        "Motion", "AutoSpeed", motion.auto_speed), 0.01f, 1.0f);
    motion.sweep_mode = static_cast<control::SweepMode>(std::clamp(
        reader.integer("Motion", "SweepMode",
                       static_cast<int>(motion.sweep_mode)), 0, 2));

    auto &labels = ui::label_settings();
    labels = ui::LabelSettings{};
    labels.visible = reader.boolean("Labels", "Visible", labels.visible);
    reader.text("Labels", "BeforeText", labels.before_text);
    reader.text("Labels", "AfterText", labels.after_text);
    labels.left_x = std::clamp(reader.number(
        "Labels", "LeftX", labels.left_x), 0.0f, 1.0f);
    labels.left_y = std::clamp(reader.number(
        "Labels", "LeftY", labels.left_y), 0.0f, 1.0f);
    labels.right_x = std::clamp(reader.number(
        "Labels", "RightX", labels.right_x), 0.0f, 1.0f);
    labels.right_y = std::clamp(reader.number(
        "Labels", "RightY", labels.right_y), 0.0f, 1.0f);
    labels.font_size = std::clamp(reader.number(
        "Labels", "FontSize", labels.font_size), 8.0f, 192.0f);
    labels.opacity = std::clamp(reader.number(
        "Labels", "Opacity", labels.opacity), 0.0f, 1.0f);
    labels.outline_width = std::clamp(reader.number(
        "Labels", "OutlineWidth", labels.outline_width), 0.0f, 8.0f);
    labels.outline_opacity = std::clamp(reader.number(
        "Labels", "OutlineOpacity", labels.outline_opacity), 0.0f, 1.0f);

    auto &keys = input::hotkey_bindings();
    keys = input::HotkeyBindings{};
    keys.move_left = read_chord(reader, "MoveLeft", keys.move_left);
    keys.move_right = read_chord(reader, "MoveRight", keys.move_right);
    keys.focus_left = read_chord(reader, "FocusLeft", keys.focus_left);
    keys.focus_right = read_chord(reader, "FocusRight", keys.focus_right);
    keys.toggle_comparison = read_chord(
        reader, "ToggleComparison", keys.toggle_comparison);
    keys.toggle_auto = read_chord(
        reader, "ToggleAutosweep", keys.toggle_auto);
    keys.toggle_freeze = read_chord(
        reader, "ToggleFreeze", keys.toggle_freeze);
    keys.toggle_display_mode = read_chord(
        reader, "ToggleDisplayMode", keys.toggle_display_mode);
    keys.toggle_border = read_chord(
        reader, "ToggleBorder", keys.toggle_border);

    auto &items = hud::indicators();
    items.clear();
    const int count = std::clamp(
        reader.integer("HUD", "Count", 0), 0,
        static_cast<int>(hud::maximum_indicators));
    items.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index)
    {
        hud::Indicator item;
        const std::string section = "HUD." + std::to_string(index);
        item.enabled = reader.boolean(
            section.c_str(), "Enabled", item.enabled);
        reader.text(section.c_str(), "Name", item.name);
        item.source = static_cast<hud::IndicatorSource>(std::clamp(
            reader.integer(section.c_str(), "Source",
                           static_cast<int>(item.source)), 0, 1));
        item.hotkey_chord = static_cast<std::uint32_t>(normalize_chord(
            reader.integer(section.c_str(), "HotkeyChord", 0),
            ImGuiKey_None));
        reader.text(section.c_str(), "TextOn", item.text_on);
        reader.text(section.c_str(), "TextOff", item.text_off);
        item.initial_on = reader.boolean(
            section.c_str(), "InitialOn", item.initial_on);
        item.x = std::clamp(reader.number(
            section.c_str(), "X", item.x), 0.0f, 1.0f);
        item.y = std::clamp(reader.number(
            section.c_str(), "Y", item.y), 0.0f, 1.0f);
        item.show_seconds = std::clamp(reader.number(
            section.c_str(), "ShowSeconds", item.show_seconds), 0.0f, 60.0f);
        hud::reset_indicator_runtime(item);
        items.push_back(item);
    }
    input::cancel_hotkey_capture();
}
}
