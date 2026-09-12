#include <imgui.h>

#include "ui/label_overlay.hpp"

#include "capture/reshade_capture.hpp"
#include "config/config_runtime.hpp"
#include "control/split_motion.hpp"
#include "hud/indicator_overlay.hpp"
#include "input/hotkeys.hpp"
#include "render/compositor.hpp"

#include <algorithm>
#include <cmath>

namespace framecompare::ui
{
namespace
{
LabelSettings g_label_settings;

ImU32 white_with_alpha(float opacity) noexcept
{
    const auto alpha = static_cast<int>(
        std::lround(std::clamp(opacity, 0.0f, 1.0f) * 255.0f));
    return IM_COL32(255, 255, 255, alpha);
}

ImU32 black_with_alpha(float opacity) noexcept
{
    const auto alpha = static_cast<int>(
        std::lround(std::clamp(opacity, 0.0f, 1.0f) * 255.0f));
    return IM_COL32(0, 0, 0, alpha);
}

void draw_one_label(ImDrawList *draw_list, const ResolvedLabel &label,
                    const ResolvedLabels &layout, const ImVec2 &display_size,
                    float clip_min_x, float clip_max_x)
{
    if (label.text == nullptr || label.text[0] == '\0' ||
        clip_max_x <= clip_min_x)
        return;

    draw_list->PushClipRect(
        ImVec2(clip_min_x * display_size.x, 0.0f),
        ImVec2(clip_max_x * display_size.x, display_size.y), true);

    const float current_font_size = std::max(ImGui::GetFontSize(), 1.0f);
    ImVec2 text_size = ImGui::CalcTextSize(label.text);
    const float font_scale = layout.font_size / current_font_size;
    text_size.x *= font_scale;
    text_size.y *= font_scale;

    const float outline = layout.outline_width;
    const auto placement = place_label(
        label, text_size.x, text_size.y, display_size.x, display_size.y,
        12.0f + outline);
    const ImVec2 position(placement[0], placement[1]);

    if (outline > 0.0f && layout.outline_opacity > 0.0f)
    {
        const ImU32 outline_color = black_with_alpha(layout.outline_opacity);
        const ImVec2 offsets[] = {
            {-outline, -outline}, {0.0f, -outline}, {outline, -outline},
            {-outline, 0.0f},                         {outline, 0.0f},
            {-outline, outline},  {0.0f, outline},  {outline, outline}};
        for (const ImVec2 &outline_offset : offsets)
        {
            draw_list->AddText(ImGui::GetFont(), layout.font_size,
                ImVec2(position.x + outline_offset.x,
                       position.y + outline_offset.y),
                outline_color, label.text);
        }
    }

    draw_list->AddText(ImGui::GetFont(), layout.font_size, position,
                       white_with_alpha(layout.opacity), label.text);
    draw_list->PopClipRect();
}
}

LabelSettings &label_settings() noexcept
{
    return g_label_settings;
}

void draw_labels(reshade::api::effect_runtime *runtime)
{
    auto &compositor = render::settings();
    const auto controls = input::update_controls(
        control::split_motion(), compositor);
    if (controls.comparison_disabled)
        capture::reset_runtime_state(runtime);
    hud::update_and_draw_indicators(runtime);
    config::tick();
    const ResolvedLabels layout = resolve_labels(
        g_label_settings, compositor.before_on_left);
    if (!compositor.enabled || !layout.visible)
        return;

    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    if (display_size.x <= 0.0f || display_size.y <= 0.0f)
        return;

    ImDrawList *const draw_list = ImGui::GetForegroundDrawList();
    const LabelClipRegions clips =
        resolve_label_clip_regions(compositor.split_position);
    draw_one_label(draw_list, layout.left, layout, display_size,
                   clips.left_min_x, clips.left_max_x);
    draw_one_label(draw_list, layout.right, layout, display_size,
                   clips.right_min_x, clips.right_max_x);
}
}
