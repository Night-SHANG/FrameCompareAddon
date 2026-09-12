#include <imgui.h>
#include <reshade.hpp>

#include "input/hotkeys.hpp"
#include "ui/i18n/localization.hpp"

#include <string>

namespace framecompare::input
{
namespace
{
HotkeyBindings g_bindings;
std::string g_capture_label;
int g_last_update_frame = -1;

ImGuiKey chord_key(ImGuiKeyChord chord) noexcept
{
    return static_cast<ImGuiKey>(chord & ~ImGuiMod_Mask_);
}

bool modifiers_match(ImGuiKeyChord chord, const ImGuiIO &io) noexcept
{
    return ((chord & ImGuiMod_Ctrl) != 0) == io.KeyCtrl &&
           ((chord & ImGuiMod_Shift) != 0) == io.KeyShift &&
           ((chord & ImGuiMod_Alt) != 0) == io.KeyAlt &&
           ((chord & ImGuiMod_Super) != 0) == io.KeySuper;
}

bool chord_down(ImGuiKeyChord chord) noexcept
{
    const ImGuiKey key = chord_key(chord);
    return key != ImGuiKey_None && modifiers_match(chord, ImGui::GetIO()) &&
           ImGui::IsKeyDown(key);
}

bool chord_pressed(ImGuiKeyChord chord) noexcept
{
    const ImGuiKey key = chord_key(chord);
    return key != ImGuiKey_None && modifiers_match(chord, ImGui::GetIO()) &&
           ImGui::IsKeyPressed(key, false);
}

bool is_modifier_key(ImGuiKey key) noexcept
{
    return key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl ||
           key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift ||
           key == ImGuiKey_LeftAlt || key == ImGuiKey_RightAlt ||
           key == ImGuiKey_LeftSuper || key == ImGuiKey_RightSuper;
}

bool is_keyboard_key(ImGuiKey key) noexcept
{
    return key >= ImGuiKey_NamedKey_BEGIN && key < ImGuiKey_GamepadStart &&
           !(key >= ImGuiKey_MouseLeft && key <= ImGuiKey_MouseWheelY);
}

ImGuiKeyChord capture_pressed_chord() noexcept
{
    for (int value = ImGuiKey_NamedKey_BEGIN;
         value < ImGuiKey_NamedKey_END; ++value)
    {
        const auto key = static_cast<ImGuiKey>(value);
        if (!is_keyboard_key(key) || is_modifier_key(key) ||
            !ImGui::IsKeyPressed(key, false))
            continue;

        ImGuiKeyChord chord = key;
        const ImGuiIO &io = ImGui::GetIO();
        if (io.KeyCtrl)
            chord |= ImGuiMod_Ctrl;
        if (io.KeyShift)
            chord |= ImGuiMod_Shift;
        if (io.KeyAlt)
            chord |= ImGuiMod_Alt;
        if (io.KeySuper)
            chord |= ImGuiMod_Super;
        return chord;
    }
    return ImGuiKey_None;
}

std::string chord_name(ImGuiKeyChord chord)
{
    const ImGuiKey key = chord_key(chord);
    if (key == ImGuiKey_None)
        return ui::i18n::text(ui::i18n::TextId::unbound);

    std::string result;
    if ((chord & ImGuiMod_Ctrl) != 0)
        result += "Ctrl+";
    if ((chord & ImGuiMod_Shift) != 0)
        result += "Shift+";
    if ((chord & ImGuiMod_Alt) != 0)
        result += "Alt+";
    if ((chord & ImGuiMod_Super) != 0)
        result += "Super+";
    const char *const key_name = ImGui::GetKeyName(key);
    result += key_name != nullptr && key_name[0] != '\0' ? key_name : "Unknown";
    return result;
}
}

HotkeyBindings &hotkey_bindings() noexcept
{
    return g_bindings;
}

bool hotkey_capture_active() noexcept
{
    return !g_capture_label.empty();
}

void cancel_hotkey_capture() noexcept
{
    g_capture_label.clear();
}

bool binding_pressed(ImGuiKeyChord binding) noexcept
{
    return !hotkey_capture_active() && !ImGui::GetIO().WantCaptureKeyboard &&
           chord_pressed(binding);
}

bool draw_binding_editor(const char *label, const char *stable_id,
                         ImGuiKeyChord &binding)
{
    bool changed = false;
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    const std::string name = chord_name(binding);
    const std::string button_label = name + "##binding-" + stable_id;
    if (ImGui::Button(button_label.c_str()))
        g_capture_label = stable_id;
    ImGui::SameLine();
    const std::string clear_label = ui::i18n::label(
        ui::i18n::TextId::clear, std::string("clear-") + stable_id);
    if (ImGui::Button(clear_label.c_str()))
    {
        binding = ImGuiKey_None;
        if (g_capture_label == stable_id)
            g_capture_label.clear();
        changed = true;
    }

    if (g_capture_label == stable_id)
    {
        ImGui::SameLine();
        ImGui::TextUnformatted(ui::i18n::text(ui::i18n::TextId::press_key));
        const ImGuiKeyChord captured = capture_pressed_chord();
        if (captured != ImGuiKey_None)
        {
            binding = captured;
            g_capture_label.clear();
            changed = true;
        }
    }
    return changed;
}

ControlUpdateResult update_controls(
    control::SplitMotionController &controller,
    render::CompositorSettings &compositor)
{
    ControlUpdateResult result;
    const int frame = ImGui::GetFrameCount();
    if (frame == g_last_update_frame)
        return result;
    g_last_update_frame = frame;

    const ImGuiIO &io = ImGui::GetIO();
    if (hotkey_capture_active() || io.WantCaptureKeyboard)
        return result;

    if (chord_pressed(g_bindings.toggle_comparison))
        result.comparison_disabled = !render::toggle_enabled(compositor);
    if (chord_pressed(g_bindings.toggle_display_mode))
        render::toggle_display_mode(compositor);
    if (chord_pressed(g_bindings.toggle_border))
        render::toggle_border(compositor);
    if (chord_pressed(g_bindings.toggle_auto))
        controller.toggle(compositor.split_position);
    if (chord_pressed(g_bindings.toggle_freeze))
        controller.settings().frozen = !controller.settings().frozen;

    controller.update(compositor.split_position, io.DeltaTime,
                      chord_down(g_bindings.move_left),
                      chord_down(g_bindings.move_right));
    render::move_center_focus(
        compositor, io.DeltaTime, controller.settings().manual_speed,
        chord_down(g_bindings.focus_left),
        chord_down(g_bindings.focus_right));
    return result;
}
}
