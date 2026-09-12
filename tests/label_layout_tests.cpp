#include "ui/label_layout.hpp"

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
}

int main()
{
    semantic_labels_follow_their_current_side();
    layout_values_are_safe_for_the_viewport();
    std::cout << "label_layout_tests: PASS\n";
}
