#include "hud/indicator_model.hpp"

#include <algorithm>

namespace framecompare::hud
{
namespace
{
std::vector<Indicator> g_indicators;
}

void update_indicator(Indicator &indicator, double now_seconds,
                      bool hotkey_pressed, bool reshade_effects_enabled)
{
    if (!indicator.runtime_initialized)
    {
        indicator.runtime_on =
            indicator.source == IndicatorSource::reshade_effects
            ? reshade_effects_enabled
            : indicator.initial_on;
        indicator.runtime_initialized = true;
        indicator.changed_at_seconds = now_seconds;
    }

    bool next = indicator.runtime_on;
    if (indicator.source == IndicatorSource::reshade_effects)
        next = reshade_effects_enabled;
    else if (hotkey_pressed)
        next = !indicator.runtime_on;

    if (next != indicator.runtime_on)
    {
        indicator.runtime_on = next;
        indicator.changed_at_seconds = now_seconds;
    }
}

bool indicator_visible(const Indicator &indicator,
                       double now_seconds) noexcept
{
    if (!indicator.enabled || !indicator.runtime_initialized)
        return false;
    const float duration = std::clamp(indicator.show_seconds, 0.0f, 60.0f);
    return duration == 0.0f ||
        now_seconds - indicator.changed_at_seconds <= duration;
}

const char *indicator_text(const Indicator &indicator) noexcept
{
    return indicator.runtime_on
        ? indicator.text_on.data()
        : indicator.text_off.data();
}

void reset_indicator_runtime(Indicator &indicator) noexcept
{
    indicator.runtime_initialized = false;
    indicator.runtime_on = indicator.initial_on;
    indicator.changed_at_seconds = 0.0;
}

std::vector<Indicator> &indicators() noexcept
{
    return g_indicators;
}

void reset_all_indicator_runtime() noexcept
{
    for (Indicator &indicator : g_indicators)
        reset_indicator_runtime(indicator);
}
}
