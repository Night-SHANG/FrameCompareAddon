#pragma once

#include "control/split_motion.hpp"

#include <imgui.h>

namespace framecompare::input
{
struct HotkeyBindings
{
    ImGuiKeyChord move_left = ImGuiMod_Ctrl | ImGuiKey_LeftArrow;
    ImGuiKeyChord move_right = ImGuiMod_Ctrl | ImGuiKey_RightArrow;
    ImGuiKeyChord toggle_auto = ImGuiMod_Ctrl | ImGuiKey_F8;
    ImGuiKeyChord toggle_freeze = ImGuiMod_Ctrl | ImGuiKey_F9;
};

HotkeyBindings &hotkey_bindings() noexcept;
bool hotkey_capture_active() noexcept;
void draw_binding_editor(const char *label, ImGuiKeyChord &binding);
void update_controls(control::SplitMotionController &controller,
                     float &split_position);
}
