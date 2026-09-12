#include "capture/reshade_capture.hpp"

namespace framecompare::capture
{
using namespace reshade::api;

namespace
{
void destroy_texture(device *device, CaptureTexture &texture)
{
    if (texture.view != 0)
        device->destroy_resource_view(texture.view);
    if (texture.resource != 0)
        device->destroy_resource(texture.resource);
    texture = {};
}

bool compatible(const CaptureTexture &texture, const resource_desc &source)
{
    return texture.resource != 0 &&
        texture.width == source.texture.width &&
        texture.height == source.texture.height &&
        texture.format == source.texture.format;
}

bool ensure_texture(effect_runtime *runtime, CaptureTexture &texture,
                    resource source, const char *debug_name)
{
    device *const device = runtime->get_device();
    const resource_desc source_desc = device->get_resource_desc(source);
    if ((source_desc.type != resource_type::texture_2d &&
         source_desc.type != resource_type::surface) ||
        source_desc.texture.samples != 1)
        return false;

    if (compatible(texture, source_desc))
        return true;

    if (texture.resource != 0 || texture.view != 0)
    {
        runtime->get_command_queue()->wait_idle();
        destroy_texture(device, texture);
    }

    const format typed = format_to_default_typed(source_desc.texture.format);
    if (typed == format::unknown ||
        !device->check_format_support(typed, resource_usage::shader_resource))
        return false;

    const resource_desc capture_desc(
        source_desc.texture.width, source_desc.texture.height, 1, 1,
        source_desc.texture.format, 1, memory_heap::default_,
        resource_usage::copy_dest | resource_usage::shader_resource);

    if (!device->create_resource(capture_desc, nullptr, resource_usage::copy_dest,
                                 &texture.resource))
        return false;
    if (!device->create_resource_view(texture.resource,
                                      resource_usage::shader_resource,
                                      resource_view_desc(typed), &texture.view))
    {
        device->destroy_resource(texture.resource);
        texture = {};
        return false;
    }

    device->set_resource_name(texture.resource, debug_name);
    texture.width = source_desc.texture.width;
    texture.height = source_desc.texture.height;
    texture.format = source_desc.texture.format;
    return true;
}

bool copy_target(effect_runtime *runtime, command_list *commands,
                 resource_view target, CaptureTexture &destination,
                 const char *debug_name)
{
    if (commands == nullptr || target == 0)
        return false;

    device *const device = runtime->get_device();
    const resource source = device->get_resource_from_view(target);
    if (source == 0 || !ensure_texture(runtime, destination, source, debug_name))
        return false;

    if (destination.shader_resource_state)
        commands->barrier(destination.resource, resource_usage::shader_resource,
                          resource_usage::copy_dest);
    commands->barrier(source, resource_usage::render_target,
                      resource_usage::copy_source);
    commands->copy_resource(source, destination.resource);
    commands->barrier(source, resource_usage::copy_source,
                      resource_usage::render_target);
    commands->barrier(destination.resource, resource_usage::copy_dest,
                      resource_usage::shader_resource);

    destination.shader_resource_state = true;
    destination.ready = true;
    return true;
}

CaptureStamp make_stamp(FrameToken token, CaptureProvenance provenance)
{
    return {token.runtime_id, token.frame_sequence, provenance};
}
}

RuntimeCaptureState *state_for(effect_runtime *runtime)
{
    return runtime->get_private_data<RuntimeCaptureState>();
}

void on_init_runtime(effect_runtime *runtime)
{
    RuntimeCaptureState *const state =
        runtime->create_private_data<RuntimeCaptureState>();
    state->cycle.emplace(reinterpret_cast<std::uintptr_t>(runtime));
}

void on_destroy_runtime(effect_runtime *runtime)
{
    RuntimeCaptureState *const state =
        runtime->get_private_data<RuntimeCaptureState>();
    if (state == nullptr)
        return;

    runtime->get_command_queue()->wait_idle();
    device *const device = runtime->get_device();
    destroy_texture(device, state->before);
    destroy_texture(device, state->after);
    runtime->destroy_private_data<RuntimeCaptureState>();
}

void on_reloaded_effects(effect_runtime *runtime)
{
    RuntimeCaptureState *const state =
        runtime->get_private_data<RuntimeCaptureState>();
    if (state == nullptr)
        return;
    state->pair.reset();
    if (state->cycle)
        state->cycle->reset();
    state->before.ready = false;
    state->after.ready = false;
}

void on_begin_effects(effect_runtime *runtime, command_list *commands,
                      resource_view target, resource_view)
{
    RuntimeCaptureState *const state =
        runtime->get_private_data<RuntimeCaptureState>();
    if (state == nullptr || !state->cycle)
        return;

    const FrameToken token = state->cycle->begin();
    if (!token.valid() ||
        !copy_target(runtime, commands, target, state->before,
                     "FrameCompare v2 Pre-ReShade FX"))
    {
        state->cycle->finish();
        state->pair.reset();
        return;
    }

    state->pair.submit_before(
        make_stamp(token, CaptureProvenance::pre_reshade_fx));
}

void on_finish_effects(effect_runtime *runtime, command_list *commands,
                       resource_view target, resource_view)
{
    RuntimeCaptureState *const state =
        runtime->get_private_data<RuntimeCaptureState>();
    if (state == nullptr || !state->cycle)
        return;

    const FrameToken token = state->cycle->finish();
    if (!token.valid() ||
        !copy_target(runtime, commands, target, state->after,
                     "FrameCompare v2 Post-ReShade FX"))
    {
        state->pair.reset();
        return;
    }

    state->pair.submit_after(
        make_stamp(token, CaptureProvenance::post_reshade_fx));
}
}
