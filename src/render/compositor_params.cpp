#include "render/compositor_params.hpp"

#include <algorithm>

namespace framecompare::render
{
ShaderParams make_shader_params(const CompositorSettings &settings,
                                bool pair_ready) noexcept
{
    ShaderParams result;
    result.split_position = std::clamp(settings.split_position, 0.0f, 1.0f);
    result.border_width = std::max(settings.border_width, 0.0f);
    result.border_opacity = std::clamp(settings.border_opacity, 0.0f, 1.0f);
    result.show_border = settings.show_border ? 1.0f : 0.0f;
    result.before_on_left = settings.before_on_left ? 1.0f : 0.0f;
    result.display_mode = static_cast<float>(settings.display_mode);
    result.pair_ready = settings.enabled && pair_ready ? 1.0f : 0.0f;
    result.center_focus = std::clamp(settings.center_focus, 0.0f, 1.0f);
    return result;
}

bool toggle_enabled(CompositorSettings &settings) noexcept
{
    settings.enabled = !settings.enabled;
    return settings.enabled;
}

void toggle_display_mode(CompositorSettings &settings) noexcept
{
    settings.display_mode = settings.display_mode ==
            DisplayMode::same_coordinate_wipe
        ? DisplayMode::center_remap
        : DisplayMode::same_coordinate_wipe;
}

void toggle_border(CompositorSettings &settings) noexcept
{
    settings.show_border = !settings.show_border;
}

void move_center_focus(CompositorSettings &settings, float delta_seconds,
                       float speed, bool move_left,
                       bool move_right) noexcept
{
    const int direction = static_cast<int>(move_right) -
                          static_cast<int>(move_left);
    const float step = std::max(delta_seconds, 0.0f) *
                       std::max(speed, 0.0f);
    settings.center_focus = std::clamp(
        settings.center_focus + static_cast<float>(direction) * step,
        0.0f, 1.0f);
}
}
