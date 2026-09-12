#include <imgui.h>

#include "ui/label_overlay.hpp"

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
                    const ResolvedLabels &layout, const ImVec2 &display_size)
{
    if (label.text == nullptr || label.text[0] == '\0')
        return;

    const float current_font_size = std::max(ImGui::GetFontSize(), 1.0f);
    ImVec2 text_size = ImGui::CalcTextSize(label.text);
    const float font_scale = layout.font_size / current_font_size;
    text_size.x *= font_scale;
    text_size.y *= font_scale;

    ImVec2 position(label.x * display_size.x, label.y * display_size.y);
    if (label.align_right)
        position.x -= text_size.x;
    position.x = std::clamp(position.x, 0.0f,
                            std::max(display_size.x - text_size.x, 0.0f));
    position.y = std::clamp(position.y, 0.0f,
                            std::max(display_size.y - text_size.y, 0.0f));

    if (layout.outline_width > 0.0f && layout.outline_opacity > 0.0f)
    {
        const ImU32 outline_color = black_with_alpha(layout.outline_opacity);
        const float offset = layout.outline_width;
        const ImVec2 offsets[] = {
            {-offset, -offset}, {0.0f, -offset}, {offset, -offset},
            {-offset, 0.0f},                       {offset, 0.0f},
            {-offset, offset},  {0.0f, offset},  {offset, offset}};
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
}
}

LabelSettings &label_settings() noexcept
{
    return g_label_settings;
}

void draw_labels(reshade::api::effect_runtime *)
{
    const auto &compositor = render::settings();
    const ResolvedLabels layout = resolve_labels(
        g_label_settings, compositor.before_on_left);
    if (!compositor.enabled || !layout.visible)
        return;

    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    if (display_size.x <= 0.0f || display_size.y <= 0.0f)
        return;

    ImDrawList *const draw_list = ImGui::GetForegroundDrawList();
    draw_one_label(draw_list, layout.left, layout, display_size);
    draw_one_label(draw_list, layout.right, layout, display_size);
}
}
