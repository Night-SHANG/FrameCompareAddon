#include <imgui.h>

#include "capture/reshade_capture.hpp"
#include "config/config_runtime.hpp"
#include "control/split_motion.hpp"
#include "integrations/dlss5/center/bridge.hpp"
#include "integrations/dlss5/hook_manager.hpp"
#include "integrations/dlss5/runtime.hpp"
#include "render/compositor.hpp"
#include "ui/label_overlay.hpp"
#include "ui/panel.hpp"

#include <Windows.h>
#include <reshade.hpp>

#if !defined(RESHADE_API_VERSION) || RESHADE_API_VERSION < 20
#error "FrameCompare v2 requires ReShade Add-on API 20 or newer."
#endif

extern "C" __declspec(dllexport) const char *NAME = "FrameCompare v2";
extern "C" __declspec(dllexport) const char *AUTHOR = "Night Shang";
extern "C" __declspec(dllexport) const char *DESCRIPTION =
    "Same-frame realtime Before/After comparison with explicit capture provenance.";

namespace
{
void publish_dlss5_settings()
{
    const auto &settings = framecompare::render::settings();
    framecompare::dlss5::Operation operation =
        framecompare::dlss5::Operation::disabled;
    if (settings.enabled && settings.dlss5_before)
        operation = settings.display_mode ==
                framecompare::render::DisplayMode::same_coordinate_wipe
            ? framecompare::dlss5::Operation::same_coordinate_region
            : framecompare::dlss5::Operation::center_full_frame;
    framecompare::dlss5::publish_settings({
        operation, settings.split_position, settings.before_on_left});
}

void draw_panel(reshade::api::effect_runtime *runtime)
{
    framecompare::ui::draw_panel(runtime);
    publish_dlss5_settings();
}

void on_init_runtime(reshade::api::effect_runtime *runtime)
{
    framecompare::capture::on_init_runtime(runtime);
    framecompare::render::on_init_runtime(runtime);
    framecompare::dlss5::center::attach_runtime(runtime);
}

void on_destroy_runtime(reshade::api::effect_runtime *runtime)
{
    framecompare::render::on_destroy_runtime(runtime);
    framecompare::capture::on_destroy_runtime(runtime);
    framecompare::dlss5::center::detach_runtime(runtime);
}

void on_reloaded_effects(reshade::api::effect_runtime *runtime)
{
    framecompare::capture::on_reloaded_effects(runtime);
    framecompare::render::on_reloaded_effects(runtime);
}

void on_begin_effects(reshade::api::effect_runtime *runtime,
                      reshade::api::command_list *commands,
                      reshade::api::resource_view target,
                      reshade::api::resource_view target_srgb)
{
    framecompare::render::prepare_cycle(runtime);
    publish_dlss5_settings();
    const auto *state = framecompare::capture::state_for(runtime);
    const bool frozen = framecompare::control::split_motion().settings().frozen;
    const bool pair_ready = state != nullptr && state->pair.ready();
    if (framecompare::render::settings().enabled &&
        framecompare::control::should_capture_pair(frozen, pair_ready))
        framecompare::capture::on_begin_effects(runtime, commands, target,
                                                target_srgb);
}

void on_finish_effects(reshade::api::effect_runtime *runtime,
                       reshade::api::command_list *commands,
                       reshade::api::resource_view target,
                       reshade::api::resource_view target_srgb)
{
    if (!framecompare::render::settings().enabled)
        return;
    const auto *current = framecompare::capture::state_for(runtime);
    const bool frozen = framecompare::control::split_motion().settings().frozen;
    const bool pair_ready = current != nullptr && current->pair.ready();
    if (framecompare::control::should_capture_pair(frozen, pair_ready))
        framecompare::capture::on_finish_effects(runtime, commands, target,
                                                 target_srgb);
    if (const auto *state = framecompare::capture::state_for(runtime))
        framecompare::render::draw(runtime, commands, target, target_srgb,
                                   *state);
}
}

extern "C" __declspec(dllexport) bool AddonInit(HMODULE addon_module,
                                                  HMODULE reshade_module)
{
    if (!reshade::register_addon(addon_module, reshade_module))
        return false;

    framecompare::config::initialize(addon_module);
    publish_dlss5_settings();
    framecompare::dlss5::start_hooks();

    reshade::register_event<reshade::addon_event::init_effect_runtime>(
        on_init_runtime);
    reshade::register_event<reshade::addon_event::destroy_effect_runtime>(
        on_destroy_runtime);
    reshade::register_event<reshade::addon_event::reshade_begin_effects>(
        on_begin_effects);
    reshade::register_event<reshade::addon_event::reshade_finish_effects>(
        on_finish_effects);
    reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(
        on_reloaded_effects);
    reshade::register_overlay(nullptr, draw_panel);
    reshade::register_overlay("OSD", framecompare::ui::draw_labels);
    return true;
}

extern "C" __declspec(dllexport) void AddonUninit(HMODULE addon_module,
                                                    HMODULE reshade_module)
{
    framecompare::dlss5::publish_settings({
        framecompare::dlss5::Operation::disabled, 0.5f, true});
    framecompare::dlss5::stop_hooks();
    framecompare::dlss5::center::shutdown();
    framecompare::config::shutdown();
    reshade::unregister_overlay("OSD", framecompare::ui::draw_labels);
    reshade::unregister_overlay(nullptr, draw_panel);
    reshade::unregister_event<reshade::addon_event::reshade_reloaded_effects>(
        on_reloaded_effects);
    reshade::unregister_event<reshade::addon_event::reshade_finish_effects>(
        on_finish_effects);
    reshade::unregister_event<reshade::addon_event::reshade_begin_effects>(
        on_begin_effects);
    reshade::unregister_event<reshade::addon_event::destroy_effect_runtime>(
        on_destroy_runtime);
    reshade::unregister_event<reshade::addon_event::init_effect_runtime>(
        on_init_runtime);
    reshade::unregister_addon(addon_module, reshade_module);
}
