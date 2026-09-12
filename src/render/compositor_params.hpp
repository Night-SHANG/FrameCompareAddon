#pragma once

#include <cstdint>

namespace framecompare::render
{
enum class DisplayMode : std::uint8_t
{
    same_coordinate_wipe,
    center_remap,
};

struct CompositorSettings
{
    bool enabled = true;
    bool dlss5_before = false;
    bool before_on_left = true;
    bool show_border = true;
    float split_position = 0.5f;
    float center_focus = 0.5f;
    float border_width = 0.002f;
    float border_opacity = 1.0f;
    DisplayMode display_mode = DisplayMode::same_coordinate_wipe;
};

struct ShaderParams
{
    float split_position = 0.5f;
    float border_width = 0.002f;
    float border_opacity = 1.0f;
    float show_border = 1.0f;
    float before_on_left = 1.0f;
    float display_mode = 0.0f;
    float pair_ready = 0.0f;
    float center_focus = 0.5f;
};

ShaderParams make_shader_params(const CompositorSettings &settings,
                                bool pair_ready) noexcept;
bool toggle_enabled(CompositorSettings &settings) noexcept;
void toggle_display_mode(CompositorSettings &settings) noexcept;
void toggle_border(CompositorSettings &settings) noexcept;
void move_center_focus(CompositorSettings &settings, float delta_seconds,
                       float speed, bool move_left,
                       bool move_right) noexcept;
}
