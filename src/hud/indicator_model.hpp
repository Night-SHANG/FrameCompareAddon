#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace framecompare::hud
{
inline constexpr std::size_t indicator_name_capacity = 64;
inline constexpr std::size_t indicator_text_capacity = 128;
inline constexpr std::size_t maximum_indicators = 32;

enum class IndicatorSource : std::uint8_t
{
    tracked_hotkey,
    reshade_effects,
};

struct Indicator
{
    bool enabled = true;
    std::array<char, indicator_name_capacity> name = {
        'I', 'n', 'd', 'i', 'c', 'a', 't', 'o', 'r', '\0'};
    IndicatorSource source = IndicatorSource::tracked_hotkey;
    std::uint32_t hotkey_chord = 0;
    std::array<char, indicator_text_capacity> text_on = {'O', 'N', '\0'};
    std::array<char, indicator_text_capacity> text_off = {
        'O', 'F', 'F', '\0'};
    bool initial_on = false;
    float x = 0.5f;
    float y = 0.12f;
    float show_seconds = 1.5f;

    bool runtime_initialized = false;
    bool runtime_on = false;
    double changed_at_seconds = 0.0;
};

void update_indicator(Indicator &indicator, double now_seconds,
                      bool hotkey_pressed, bool reshade_effects_enabled);
bool indicator_visible(const Indicator &indicator,
                       double now_seconds) noexcept;
const char *indicator_text(const Indicator &indicator) noexcept;
void reset_indicator_runtime(Indicator &indicator) noexcept;
std::array<float, 2> place_indicator(
    float normalized_x, float normalized_y,
    float text_width, float text_height,
    float display_width, float display_height,
    float safe_margin) noexcept;

std::vector<Indicator> &indicators() noexcept;
void reset_all_indicator_runtime() noexcept;
}
