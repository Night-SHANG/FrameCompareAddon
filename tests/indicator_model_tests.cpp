#include "hud/indicator_model.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

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

void tracked_hotkey_toggles_and_expires()
{
    framecompare::hud::Indicator indicator;
    indicator.initial_on = false;
    indicator.show_seconds = 2.0f;

    framecompare::hud::update_indicator(indicator, 10.0, false, true);
    require(!indicator.runtime_on,
            "tracked shortcut should initialize from configured state");
    require(framecompare::hud::indicator_visible(indicator, 11.0),
            "initial state should be visible for its configured duration");

    framecompare::hud::update_indicator(indicator, 12.0, true, true);
    require(indicator.runtime_on,
            "tracked shortcut should toggle state when pressed");
    require(std::string_view(framecompare::hud::indicator_text(indicator)) ==
                "ON",
            "toggled state should select ON text");
    require(!framecompare::hud::indicator_visible(indicator, 14.1),
            "timed indicator should hide after the configured duration");
}

void reshade_source_follows_actual_runtime_state()
{
    framecompare::hud::Indicator indicator;
    indicator.source = framecompare::hud::IndicatorSource::reshade_effects;
    indicator.initial_on = false;

    framecompare::hud::update_indicator(indicator, 1.0, false, true);
    require(indicator.runtime_on,
            "ReShade source should ignore configured initial state");
    framecompare::hud::update_indicator(indicator, 2.0, false, false);
    require(!indicator.runtime_on && indicator.changed_at_seconds == 2.0,
            "ReShade source should follow actual runtime changes");
}

void top_right_indicator_respects_safe_margin()
{
    const auto position = framecompare::hud::place_indicator(
        0.946f, 0.073f, 280.0f, 64.0f, 1567.0f, 881.0f, 14.0f);
    require(position[0] == 1273.0f,
            "right-aligned indicator should keep its safe margin");
    require(position[1] >= 14.0f,
            "top indicator should keep its safe margin");
}
}

int main()
{
    tracked_hotkey_toggles_and_expires();
    reshade_source_follows_actual_runtime_state();
    top_right_indicator_respects_safe_margin();
    std::cout << "indicator_model_tests: PASS\n";
}
