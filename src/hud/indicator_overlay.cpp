#include <imgui.h>

#include "hud/indicator_overlay.hpp"

#include "hud/indicator_model.hpp"
#include "input/hotkeys.hpp"
#include "ui/label_overlay.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace framecompare::hud
{
namespace
{
using Clock = std::chrono::steady_clock;

double current_time_seconds() noexcept
{
    return std::chrono::duration<double>(
        Clock::now().time_since_epoch()).count();
}

ImU32 color(unsigned char value, float opacity) noexcept
{
    const auto alpha = static_cast<int>(
        std::lround(std::clamp(opacity, 0.0f, 1.0f) * 255.0f));
    return IM_COL32(value, value, value, alpha);
}

void draw_indicator(ImDrawList *draw_list, const Indicator &indicator,
                    const ui::LabelSettings &style,
                    const ImVec2 &display_size)
{
    const char *const text = indicator_text(indicator);
    if (text == nullptr || text[0] == '\0')
        return;

    const float font_size = std::clamp(style.font_size, 8.0f, 192.0f);
    const float current_font_size = std::max(ImGui::GetFontSize(), 1.0f);
    ImVec2 text_size = ImGui::CalcTextSize(text);
    const float font_scale = font_size / current_font_size;
    text_size.x *= font_scale;
    text_size.y *= font_scale;

    const float outline = std::clamp(style.outline_width, 0.0f, 8.0f);
    const auto placement = place_indicator(
        indicator.x, indicator.y, text_size.x, text_size.y,
        display_size.x, display_size.y, 12.0f + outline);
    const ImVec2 position(placement[0], placement[1]);
    if (outline > 0.0f && style.outline_opacity > 0.0f)
    {
        const ImVec2 offsets[] = {
            {-outline, -outline}, {0.0f, -outline}, {outline, -outline},
            {-outline, 0.0f},                         {outline, 0.0f},
            {-outline, outline},  {0.0f, outline},  {outline, outline}};
        for (const ImVec2 &offset : offsets)
            draw_list->AddText(
                ImGui::GetFont(), font_size,
                ImVec2(position.x + offset.x, position.y + offset.y),
                color(0, style.outline_opacity), text);
    }
    draw_list->AddText(ImGui::GetFont(), font_size, position,
                       color(255, style.opacity), text);
}
}

void update_and_draw_indicators(reshade::api::effect_runtime *runtime)
{
    if (runtime == nullptr)
        return;
    const double now = current_time_seconds();
    const bool reshade_enabled = runtime->get_effects_state();
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    ImDrawList *const draw_list = ImGui::GetForegroundDrawList();

    for (Indicator &indicator : indicators())
    {
        const bool pressed =
            indicator.source == IndicatorSource::tracked_hotkey &&
            input::binding_pressed(
                static_cast<ImGuiKeyChord>(indicator.hotkey_chord));
        update_indicator(indicator, now, pressed, reshade_enabled);
        if (display_size.x > 0.0f && display_size.y > 0.0f &&
            indicator_visible(indicator, now))
            draw_indicator(draw_list, indicator, ui::label_settings(),
                           display_size);
    }
}
}
