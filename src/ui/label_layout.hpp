#pragma once

#include <array>
#include <cstddef>

namespace framecompare::ui
{
inline constexpr std::size_t label_text_capacity = 64;

struct LabelSettings
{
    bool visible = true;
    std::array<char, label_text_capacity> before_text = {'O', 'F', 'F', '\0'};
    std::array<char, label_text_capacity> after_text = {'O', 'N', '\0'};
    float left_x = 0.03f;
    float left_y = 0.04f;
    float right_x = 0.97f;
    float right_y = 0.04f;
    float font_size = 36.0f;
    float opacity = 1.0f;
    float outline_width = 2.0f;
    float outline_opacity = 0.85f;
};

struct ResolvedLabel
{
    const char *text = "";
    float x = 0.0f;
    float y = 0.0f;
    bool align_right = false;
};

struct ResolvedLabels
{
    bool visible = true;
    ResolvedLabel left;
    ResolvedLabel right;
    float font_size = 36.0f;
    float opacity = 1.0f;
    float outline_width = 2.0f;
    float outline_opacity = 0.85f;
};

struct LabelClipRegions
{
    float left_min_x = 0.0f;
    float left_max_x = 0.5f;
    float right_min_x = 0.5f;
    float right_max_x = 1.0f;
};

ResolvedLabels resolve_labels(const LabelSettings &settings,
                              bool before_on_left) noexcept;
LabelClipRegions resolve_label_clip_regions(float split_position) noexcept;
std::array<float, 2> place_label(
    const ResolvedLabel &label, float text_width, float text_height,
    float display_width, float display_height, float safe_margin) noexcept;
void mirror_left_to_right(LabelSettings &settings) noexcept;
void mirror_right_to_left(LabelSettings &settings) noexcept;
}
