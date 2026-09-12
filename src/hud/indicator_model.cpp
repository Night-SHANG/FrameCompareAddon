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

std::array<float, 2> place_indicator(
    float normalized_x, float normalized_y,
    float text_width, float text_height,
    float display_width, float display_height,
    float safe_margin) noexcept
{
    const float width = std::max(text_width, 0.0f);
    const float height = std::max(text_height, 0.0f);
    const float screen_width = std::max(display_width, 0.0f);
    const float screen_height = std::max(display_height, 0.0f);
    const float max_x = std::max(screen_width - width, 0.0f);
    const float max_y = std::max(screen_height - height, 0.0f);
    const float inset_x = std::clamp(safe_margin, 0.0f, max_x * 0.5f);
    const float inset_y = std::clamp(safe_margin, 0.0f, max_y * 0.5f);
    const float x = std::clamp(
        std::clamp(normalized_x, 0.0f, 1.0f) * screen_width - width * 0.5f,
        inset_x, max_x - inset_x);
    const float y = std::clamp(
        std::clamp(normalized_y, 0.0f, 1.0f) * screen_height - height * 0.5f,
        inset_y, max_y - inset_y);
    return {x, y};
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
