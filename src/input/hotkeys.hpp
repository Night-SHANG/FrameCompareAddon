#pragma once

#include "control/split_motion.hpp"
#include "render/compositor_params.hpp"

#include <imgui.h>

namespace framecompare::input
{
struct HotkeyBindings
{
    ImGuiKeyChord move_left = ImGuiMod_Ctrl | ImGuiKey_LeftArrow;
    ImGuiKeyChord move_right = ImGuiMod_Ctrl | ImGuiKey_RightArrow;
    ImGuiKeyChord toggle_comparison = ImGuiMod_Ctrl | ImGuiKey_F7;
    ImGuiKeyChord toggle_auto = ImGuiMod_Ctrl | ImGuiKey_F8;
    ImGuiKeyChord toggle_freeze = ImGuiMod_Ctrl | ImGuiKey_F9;
    ImGuiKeyChord toggle_display_mode = ImGuiMod_Ctrl | ImGuiKey_F10;
    ImGuiKeyChord toggle_border = ImGuiMod_Ctrl | ImGuiKey_F11;
};

struct ControlUpdateResult
{
    bool comparison_disabled = false;
};

HotkeyBindings &hotkey_bindings() noexcept;
bool hotkey_capture_active() noexcept;
void draw_binding_editor(const char *label, ImGuiKeyChord &binding);
ControlUpdateResult update_controls(
    control::SplitMotionController &controller,
    render::CompositorSettings &compositor);
}
