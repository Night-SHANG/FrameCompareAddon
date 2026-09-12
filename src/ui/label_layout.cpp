#include "ui/label_layout.hpp"

#include <algorithm>

namespace framecompare::ui
{
ResolvedLabels resolve_labels(const LabelSettings &settings,
                              bool before_on_left) noexcept
{
    ResolvedLabels result;
    result.visible = settings.visible;
    result.left.text = before_on_left
        ? settings.before_text.data()
        : settings.after_text.data();
    result.right.text = before_on_left
        ? settings.after_text.data()
        : settings.before_text.data();
    result.left.x = std::clamp(settings.left_x, 0.0f, 1.0f);
    result.left.y = std::clamp(settings.left_y, 0.0f, 1.0f);
    result.right.x = std::clamp(settings.right_x, 0.0f, 1.0f);
    result.right.y = std::clamp(settings.right_y, 0.0f, 1.0f);
    result.right.align_right = true;
    result.font_size = std::clamp(settings.font_size, 8.0f, 192.0f);
    result.opacity = std::clamp(settings.opacity, 0.0f, 1.0f);
    result.outline_width = std::clamp(settings.outline_width, 0.0f, 8.0f);
    result.outline_opacity = std::clamp(
        settings.outline_opacity, 0.0f, 1.0f);
    return result;
}

void mirror_left_to_right(LabelSettings &settings) noexcept
{
    const float left_x = std::clamp(settings.left_x, 0.0f, 1.0f);
    settings.right_x = 1.0f - left_x;
    settings.right_y = std::clamp(settings.left_y, 0.0f, 1.0f);
}

void mirror_right_to_left(LabelSettings &settings) noexcept
{
    const float right_x = std::clamp(settings.right_x, 0.0f, 1.0f);
    settings.left_x = 1.0f - right_x;
    settings.left_y = std::clamp(settings.right_y, 0.0f, 1.0f);
}
}
