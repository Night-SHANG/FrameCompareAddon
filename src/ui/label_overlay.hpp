#pragma once

#include "ui/label_layout.hpp"

#include <reshade.hpp>

namespace framecompare::ui
{
LabelSettings &label_settings() noexcept;
void draw_labels(reshade::api::effect_runtime *runtime);
}
