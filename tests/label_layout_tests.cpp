#include "ui/label_layout.hpp"

#include <cmath>
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

bool near(float left, float right)
{
    return std::fabs(left - right) < 0.0001f;
}

void semantic_labels_follow_their_current_side()
{
    framecompare::ui::LabelSettings settings;

    const auto before_left = framecompare::ui::resolve_labels(settings, true);
    require(std::string_view(before_left.left.text) == "OFF",
            "Before text should be on the left by default");
    require(std::string_view(before_left.right.text) == "ON",
            "After text should be on the right by default");

    const auto before_right = framecompare::ui::resolve_labels(settings, false);
    require(std::string_view(before_right.left.text) == "ON",
            "After text should move left when Before is on the right");
    require(std::string_view(before_right.right.text) == "OFF",
            "Before text should move right when sides are reversed");
}

void layout_values_are_safe_for_the_viewport()
{
    framecompare::ui::LabelSettings settings;
    settings.left_x = -0.5f;
    settings.left_y = 1.5f;
    settings.right_x = 2.0f;
    settings.right_y = -1.0f;
    settings.font_size = 500.0f;
    settings.opacity = 2.0f;
    settings.outline_width = -2.0f;
    settings.outline_opacity = -1.0f;

    const auto labels = framecompare::ui::resolve_labels(settings, true);
    require(labels.left.x == 0.0f && labels.left.y == 1.0f,
            "left anchor should clamp to the viewport");
    require(labels.right.x == 1.0f && labels.right.y == 0.0f,
            "right anchor should clamp to the viewport");
    require(labels.font_size == 192.0f,
            "font size should clamp to its safe maximum");
    require(labels.opacity == 1.0f,
            "text opacity should clamp to one");
    require(labels.outline_width == 0.0f,
            "outline width should not be negative");
    require(labels.outline_opacity == 0.0f,
            "outline opacity should clamp to zero");
}

void positions_can_be_mirrored_in_both_directions()
{
    framecompare::ui::LabelSettings settings;
    settings.left_x = 0.12f;
    settings.left_y = 0.20f;
    framecompare::ui::mirror_left_to_right(settings);
    require(near(settings.right_x, 0.88f) && near(settings.right_y, 0.20f),
            "left position should mirror horizontally to the right");

    settings.right_x = 0.91f;
    settings.right_y = 0.35f;
    framecompare::ui::mirror_right_to_left(settings);
    require(near(settings.left_x, 0.09f) && near(settings.left_y, 0.35f),
            "right position should mirror horizontally to the left");
}

void label_regions_follow_and_clamp_the_split_boundary()
{
    const auto middle = framecompare::ui::resolve_label_clip_regions(0.35f);
    require(near(middle.left_min_x, 0.0f) &&
                near(middle.left_max_x, 0.35f),
            "left label should be clipped at the split boundary");
    require(near(middle.right_min_x, 0.35f) &&
                near(middle.right_max_x, 1.0f),
            "right label should begin at the split boundary");

    const auto beyond_right =
        framecompare::ui::resolve_label_clip_regions(2.0f);
    require(near(beyond_right.left_max_x, 1.0f) &&
                near(beyond_right.right_min_x, 1.0f),
            "label regions should clamp a split beyond the right edge");

    const auto beyond_left =
        framecompare::ui::resolve_label_clip_regions(-1.0f);
    require(near(beyond_left.left_max_x, 0.0f) &&
                near(beyond_left.right_min_x, 0.0f),
            "label regions should clamp a split beyond the left edge");
}

void labels_keep_text_and_outline_inside_the_viewport()
{
    framecompare::ui::ResolvedLabel left;
    left.x = 0.0f;
    left.y = 0.0f;
    const auto left_position = framecompare::ui::place_label(
        left, 100.0f, 40.0f, 1567.0f, 881.0f, 14.0f);
    require(near(left_position[0], 14.0f) &&
                near(left_position[1], 14.0f),
            "top-left label should keep its safe margin");

    framecompare::ui::ResolvedLabel right;
    right.x = 1.0f;
    right.y = 0.0f;
    right.align_right = true;
    const auto right_position = framecompare::ui::place_label(
        right, 100.0f, 40.0f, 1567.0f, 881.0f, 14.0f);
    require(near(right_position[0], 1453.0f) &&
                near(right_position[1], 14.0f),
            "top-right label should keep its safe margin");
}
}

int main()
{
    semantic_labels_follow_their_current_side();
    layout_values_are_safe_for_the_viewport();
    positions_can_be_mirrored_in_both_directions();
    label_regions_follow_and_clamp_the_split_boundary();
    labels_keep_text_and_outline_inside_the_viewport();
    std::cout << "label_layout_tests: PASS\n";
}
