#include "render/compositor_params.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    framecompare::render::CompositorSettings settings;
    require(!settings.dlss5_before,
            "DLSS5 Before source must default to disabled");
    settings.split_position = -0.25f;
    settings.border_width = -3.0f;
    settings.border_opacity = 2.0f;
    settings.center_focus = -0.25f;

    const auto unavailable = framecompare::render::make_shader_params(settings, false);
    require(unavailable.split_position == 0.0f,
            "split position should clamp to the left edge");
    require(unavailable.border_width == 0.0f,
            "border width must not be negative");
    require(unavailable.border_opacity == 1.0f,
            "border opacity should clamp to one");
    require(unavailable.center_focus == 0.0f,
            "center focus should clamp to the left edge");
    require(unavailable.pair_ready == 0.0f,
            "composition must stay disabled without a ready pair");

    settings.split_position = 1.25f;
    settings.center_focus = 1.25f;
    const auto ready = framecompare::render::make_shader_params(settings, true);
    require(ready.split_position == 1.0f,
            "split position should clamp to the right edge");
    require(ready.pair_ready == 1.0f,
            "a ready pair should enable composition");
    require(ready.center_focus == 1.0f,
            "center focus should clamp to the right edge");

    settings.center_focus = 0.5f;
    framecompare::render::move_center_focus(
        settings, 0.5f, 0.2f, false, true);
    require(settings.center_focus == 0.6f,
            "held focus-right input should move by speed times frame time");
    framecompare::render::move_center_focus(
        settings, 10.0f, 1.0f, true, false);
    require(settings.center_focus == 0.0f,
            "center-focus movement should clamp to the left edge");

    require(!framecompare::render::toggle_enabled(settings),
            "comparison toggle should report the disabled state");
    require(!settings.enabled,
            "comparison toggle should disable an enabled comparison");
    require(framecompare::render::toggle_enabled(settings),
            "comparison toggle should report the enabled state");

    require(settings.display_mode ==
                framecompare::render::DisplayMode::same_coordinate_wipe,
            "same-coordinate wipe should be the default display mode");
    framecompare::render::toggle_display_mode(settings);
    require(settings.display_mode == framecompare::render::DisplayMode::center_remap,
            "display-mode toggle should select center remap");
    framecompare::render::toggle_display_mode(settings);
    require(settings.display_mode ==
                framecompare::render::DisplayMode::same_coordinate_wipe,
            "display-mode toggle should return to same-coordinate wipe");

    require(settings.show_border,
            "border should be visible by default");
    framecompare::render::toggle_border(settings);
    require(!settings.show_border,
            "border toggle should hide a visible border");

    using framecompare::render::center_source_center;
    require(center_source_center(0.5f, 0.0f) == 0.25f,
            "leftmost focus should keep a half-width window in bounds");
    require(center_source_center(0.5f, 0.5f) == 0.5f,
            "default focus should keep the source centered");
    require(center_source_center(0.5f, 1.0f) == 0.75f,
            "rightmost focus should keep a half-width window in bounds");
    require(center_source_center(0.8f, 0.0f) == 0.4f,
            "uneven splits should use the wider source window boundary");
    require(center_source_center(0.8f, 1.0f) == 0.6f,
            "uneven splits should stay inside the right boundary");

    std::cout << "compositor_params_tests: PASS\n";
}
