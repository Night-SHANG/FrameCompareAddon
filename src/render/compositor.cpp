#include "render/compositor.hpp"

#include <array>

namespace framecompare::render
{
using namespace reshade::api;

namespace
{
struct ParameterTexture
{
    resource resource{};
    resource_view view{};
    bool shader_resource_state = false;
};

struct __declspec(uuid("34DE0661-B69E-4DC5-82C3-95A2F26FC998")) RuntimeCompositorState
{
    effect_technique technique{};
    ParameterTexture parameters;
};

CompositorSettings g_settings;

void destroy_parameters(device *device, ParameterTexture &parameters)
{
    if (parameters.view != 0)
        device->destroy_resource_view(parameters.view);
    if (parameters.resource != 0)
        device->destroy_resource(parameters.resource);
    parameters = {};
}

void refresh(effect_runtime *runtime, RuntimeCompositorState *state)
{
    state->technique =
        runtime->find_technique("FrameCompare.fx", "FrameCompareComposite");
    if (state->technique != 0)
        runtime->set_technique_state(state->technique, false);
    if (state->parameters.view != 0)
        runtime->update_texture_bindings("FRAMECOMPARE_PARAMS",
                                         state->parameters.view,
                                         state->parameters.view);
}

bool ensure_parameters(effect_runtime *runtime, RuntimeCompositorState *state)
{
    if (state->parameters.resource != 0)
        return true;

    device *const device = runtime->get_device();
    if (!device->check_capability(device_caps::update_texture_region_command))
        return false;

    const resource_desc desc(
        2, 1, 1, 1, format::r32g32b32a32_float, 1, memory_heap::default_,
        resource_usage::copy_dest | resource_usage::shader_resource);
    if (!device->create_resource(desc, nullptr, resource_usage::copy_dest,
                                 &state->parameters.resource))
        return false;
    if (!device->create_resource_view(
            state->parameters.resource, resource_usage::shader_resource,
            resource_view_desc(format::r32g32b32a32_float),
            &state->parameters.view))
    {
        device->destroy_resource(state->parameters.resource);
        state->parameters = {};
        return false;
    }

    device->set_resource_name(state->parameters.resource,
                              "FrameCompare v2 Parameters");
    runtime->update_texture_bindings("FRAMECOMPARE_PARAMS",
                                     state->parameters.view,
                                     state->parameters.view);
    return true;
}

bool upload_parameters(effect_runtime *runtime, RuntimeCompositorState *state,
                       command_list *commands, bool pair_ready)
{
    if (commands == nullptr || !ensure_parameters(runtime, state))
        return false;

    const ShaderParams params = make_shader_params(g_settings, pair_ready);
    std::array<float, 8> values = {
        params.split_position, params.border_width,
        params.border_opacity, params.show_border,
        params.before_on_left, params.display_mode,
        params.pair_ready, params.center_focus};

    if (state->parameters.shader_resource_state)
        commands->barrier(state->parameters.resource,
                          resource_usage::shader_resource,
                          resource_usage::copy_dest);
    const subresource_data data{values.data(), sizeof(values), sizeof(values)};
    commands->update_texture_region(data, state->parameters.resource, 0);
    commands->barrier(state->parameters.resource, resource_usage::copy_dest,
                      resource_usage::shader_resource);
    state->parameters.shader_resource_state = true;
    return true;
}
}

CompositorSettings &settings() noexcept
{
    return g_settings;
}

void on_init_runtime(effect_runtime *runtime)
{
    RuntimeCompositorState *const state =
        runtime->create_private_data<RuntimeCompositorState>();
    refresh(runtime, state);
}

void on_destroy_runtime(effect_runtime *runtime)
{
    RuntimeCompositorState *const state =
        runtime->get_private_data<RuntimeCompositorState>();
    if (state == nullptr)
        return;
    runtime->get_command_queue()->wait_idle();
    destroy_parameters(runtime->get_device(), state->parameters);
    runtime->destroy_private_data<RuntimeCompositorState>();
}

void on_reloaded_effects(effect_runtime *runtime)
{
    RuntimeCompositorState *const state =
        runtime->get_private_data<RuntimeCompositorState>();
    if (state != nullptr)
        refresh(runtime, state);
}

void prepare_cycle(effect_runtime *runtime)
{
    RuntimeCompositorState *const state =
        runtime->get_private_data<RuntimeCompositorState>();
    if (state != nullptr && state->technique != 0)
        runtime->set_technique_state(state->technique, false);
}

void draw(effect_runtime *runtime, command_list *commands,
          resource_view target, resource_view target_srgb,
          const capture::RuntimeCaptureState &capture_state)
{
    RuntimeCompositorState *const state =
        runtime->get_private_data<RuntimeCompositorState>();
    if (state == nullptr || state->technique == 0 ||
        !capture_state.pair.ready() || !capture_state.before.ready ||
        !capture_state.after.ready)
        return;

    runtime->update_texture_bindings("FRAMECOMPARE_BEFORE",
                                     capture_state.before.view,
                                     capture_state.before.view);
    runtime->update_texture_bindings("FRAMECOMPARE_AFTER",
                                     capture_state.after.view,
                                     capture_state.after.view);
    if (!upload_parameters(runtime, state, commands, true))
        return;
    runtime->render_technique(state->technique, commands, target, target_srgb);
}
}
