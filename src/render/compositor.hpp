#pragma once

#include "capture/reshade_capture.hpp"
#include "render/compositor_params.hpp"

#include <reshade.hpp>

namespace framecompare::render
{
CompositorSettings &settings() noexcept;

void on_init_runtime(reshade::api::effect_runtime *runtime);
void on_destroy_runtime(reshade::api::effect_runtime *runtime);
void on_reloaded_effects(reshade::api::effect_runtime *runtime);
void prepare_cycle(reshade::api::effect_runtime *runtime);
void draw(reshade::api::effect_runtime *runtime,
          reshade::api::command_list *commands,
          reshade::api::resource_view target,
          reshade::api::resource_view target_srgb,
          const capture::RuntimeCaptureState &capture_state);
}
