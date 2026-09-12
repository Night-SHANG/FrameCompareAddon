#include "config/settings_codec.hpp"

#include "config/ini_document.hpp"
#include "control/split_motion.hpp"
#include "hud/indicator_model.hpp"
#include "input/hotkeys.hpp"
#include "render/compositor.hpp"
#include "ui/label_overlay.hpp"

#include <iomanip>
#include <sstream>

namespace framecompare::config
{
namespace
{
std::string number(float value)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(6) << value;
    return output.str();
}

std::string boolean(bool value)
{
    return value ? "1" : "0";
}

std::string chord(ImGuiKeyChord value)
{
    return std::to_string(static_cast<int>(value));
}
}

std::string encode_current_settings()
{
    IniDocument document;
    const auto &compositor = render::settings();
    document.set("General", "Enabled", boolean(compositor.enabled));
    document.set("General", "BeforeOnLeft",
                 boolean(compositor.before_on_left));
    document.set("General", "DisplayMode",
                 std::to_string(static_cast<int>(compositor.display_mode)));

    document.set("Divider", "Position", number(compositor.split_position));
    document.set("Divider", "CenterFocus", number(compositor.center_focus));
    document.set("Divider", "ShowBorder",
                 boolean(compositor.show_border));
    document.set("Divider", "BorderWidth", number(compositor.border_width));
    document.set("Divider", "BorderOpacity",
                 number(compositor.border_opacity));

    const auto &motion = control::split_motion().settings();
    document.set("Motion", "ManualSpeed", number(motion.manual_speed));
    document.set("Motion", "AutoSpeed", number(motion.auto_speed));
    document.set("Motion", "SweepMode",
                 std::to_string(static_cast<int>(motion.sweep_mode)));

    const auto &labels = ui::label_settings();
    document.set("Labels", "Visible", boolean(labels.visible));
    document.set("Labels", "BeforeText", labels.before_text.data());
    document.set("Labels", "AfterText", labels.after_text.data());
    document.set("Labels", "LeftX", number(labels.left_x));
    document.set("Labels", "LeftY", number(labels.left_y));
    document.set("Labels", "RightX", number(labels.right_x));
    document.set("Labels", "RightY", number(labels.right_y));
    document.set("Labels", "FontSize", number(labels.font_size));
    document.set("Labels", "Opacity", number(labels.opacity));
    document.set("Labels", "OutlineWidth", number(labels.outline_width));
    document.set("Labels", "OutlineOpacity",
                 number(labels.outline_opacity));

    const auto &keys = input::hotkey_bindings();
    document.set("Hotkeys", "MoveLeft", chord(keys.move_left));
    document.set("Hotkeys", "MoveRight", chord(keys.move_right));
    document.set("Hotkeys", "FocusLeft", chord(keys.focus_left));
    document.set("Hotkeys", "FocusRight", chord(keys.focus_right));
    document.set("Hotkeys", "ToggleComparison",
                 chord(keys.toggle_comparison));
    document.set("Hotkeys", "ToggleAutosweep", chord(keys.toggle_auto));
    document.set("Hotkeys", "ToggleFreeze", chord(keys.toggle_freeze));
    document.set("Hotkeys", "ToggleDisplayMode",
                 chord(keys.toggle_display_mode));
    document.set("Hotkeys", "ToggleBorder", chord(keys.toggle_border));

    const auto &items = hud::indicators();
    document.set("HUD", "Count", std::to_string(items.size()));
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        const hud::Indicator &item = items[index];
        const std::string section = "HUD." + std::to_string(index);
        document.set(section, "Enabled", boolean(item.enabled));
        document.set(section, "Name", item.name.data());
        document.set(section, "Source",
                     std::to_string(static_cast<int>(item.source)));
        document.set(section, "HotkeyChord",
                     std::to_string(item.hotkey_chord));
        document.set(section, "TextOn", item.text_on.data());
        document.set(section, "TextOff", item.text_off.data());
        document.set(section, "InitialOn", boolean(item.initial_on));
        document.set(section, "X", number(item.x));
        document.set(section, "Y", number(item.y));
        document.set(section, "ShowSeconds", number(item.show_seconds));
    }
    return document.serialize();
}
}
