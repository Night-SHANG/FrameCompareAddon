#pragma once

#include "core/capture_cycle.hpp"
#include "core/frame_pair.hpp"

#include <reshade.hpp>

#include <optional>

namespace framecompare::capture
{
struct CaptureTexture
{
    reshade::api::resource resource{};
    reshade::api::resource_view view{};
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    reshade::api::format format = reshade::api::format::unknown;
    bool shader_resource_state = false;
    bool ready = false;
};

struct __declspec(uuid("D10F0929-374E-4820-9498-3F2750A81503")) RuntimeCaptureState
{
    std::optional<CaptureCycleTracker> cycle;
    FramePairCoordinator pair;
    CaptureTexture before;
    CaptureTexture after;
};

RuntimeCaptureState *state_for(reshade::api::effect_runtime *runtime);

void on_init_runtime(reshade::api::effect_runtime *runtime);
void on_destroy_runtime(reshade::api::effect_runtime *runtime);
void on_reloaded_effects(reshade::api::effect_runtime *runtime);
void on_begin_effects(reshade::api::effect_runtime *runtime,
                      reshade::api::command_list *commands,
                      reshade::api::resource_view target,
                      reshade::api::resource_view target_srgb);
void on_finish_effects(reshade::api::effect_runtime *runtime,
                       reshade::api::command_list *commands,
                       reshade::api::resource_view target,
                       reshade::api::resource_view target_srgb);
}
