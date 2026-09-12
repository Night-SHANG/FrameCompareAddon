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
    settings.split_position = -0.25f;
    settings.border_width = -3.0f;
    settings.border_opacity = 2.0f;

    const auto unavailable = framecompare::render::make_shader_params(settings, false);
    require(unavailable.split_position == 0.0f,
            "split position should clamp to the left edge");
    require(unavailable.border_width == 0.0f,
            "border width must not be negative");
    require(unavailable.border_opacity == 1.0f,
            "border opacity should clamp to one");
    require(unavailable.pair_ready == 0.0f,
            "composition must stay disabled without a ready pair");

    settings.split_position = 1.25f;
    const auto ready = framecompare::render::make_shader_params(settings, true);
    require(ready.split_position == 1.0f,
            "split position should clamp to the right edge");
    require(ready.pair_ready == 1.0f,
            "a ready pair should enable composition");

    std::cout << "compositor_params_tests: PASS\n";
}
