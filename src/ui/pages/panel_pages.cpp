#include <imgui.h>
#include <reshade.hpp>

#include "ui/pages/panel_pages.hpp"

#include "capture/reshade_capture.hpp"
#include "config/config_runtime.hpp"
#include "control/split_motion.hpp"
#include "hud/indicator_panel.hpp"
#include "input/hotkeys.hpp"
#include "integrations/dlss5/hook_manager.hpp"
#include "integrations/dlss5/runtime.hpp"
#include "render/compositor.hpp"
#include "ui/i18n/localization.hpp"
#include "ui/label_overlay.hpp"
#include "ui/parameter_widgets.hpp"

#include <array>
#include <string_view>

namespace framecompare::ui::pages
{
namespace
{
using i18n::TextId;

const char *t(TextId id) noexcept
{
    return i18n::text(id);
}

std::string l(TextId id, const char *stable_id)
{
    return i18n::label(id, stable_id);
}

TextId config_status_text(config::ConfigStatus value) noexcept
{
    switch (value)
    {
    case config::ConfigStatus::save_failed:
        return TextId::config_save_failed;
    case config::ConfigStatus::saved:
        return TextId::config_saved;
    case config::ConfigStatus::reload_failed:
        return TextId::config_reload_failed;
    case config::ConfigStatus::reloaded:
        return TextId::config_reloaded;
    case config::ConfigStatus::none:
    default:
        return TextId::count;
    }
}
}

void draw_language_selector(const char *stable_id)
{
    int selected = i18n::language() == i18n::UiLanguage::en ? 1 : 0;
    const std::array<const char *, 2> choices = {
        t(TextId::language_chinese), t(TextId::language_english)};
    ImGui::SetNextItemWidth(130.0f);
    const std::string combo_label = l(TextId::language, stable_id);
    if (ImGui::Combo(combo_label.c_str(), &selected, choices.data(),
                     static_cast<int>(choices.size())))
    {
        i18n::set_language(selected == 1 ? i18n::UiLanguage::en
                                         : i18n::UiLanguage::zh_cn);
    }
}

void draw_compare_page(reshade::api::effect_runtime *runtime)
{
    auto &settings = render::settings();
    const std::string enabled = l(TextId::enable_comparison, "comparison-enabled");
    if (ImGui::Checkbox(enabled.c_str(), &settings.enabled) && !settings.enabled)
        capture::reset_runtime_state(runtime);

    const render::CompositorSettings defaults;
    numeric_setting(t(TextId::split_position), "split-position",
                    settings.split_position, 0.0f, 1.0f,
                    defaults.split_position, "%.3f");
    const std::string before_left = l(TextId::before_on_left, "before-on-left");
    ImGui::Checkbox(before_left.c_str(), &settings.before_on_left);

    int mode = static_cast<int>(settings.display_mode);
    const std::array<const char *, 2> modes = {
        t(TextId::mode_same_coordinate), t(TextId::mode_center_remap)};
    const std::string mode_label = l(TextId::display_mode, "display-mode");
    if (ImGui::Combo(mode_label.c_str(), &mode, modes.data(),
                     static_cast<int>(modes.size())))
        settings.display_mode = static_cast<render::DisplayMode>(mode);

    ImGui::Separator();
    const std::string dlss5_label = l(TextId::dlss5_before, "dlss5-before");
    ImGui::Checkbox(dlss5_label.c_str(), &settings.dlss5_before);
    ImGui::TextWrapped("%s", t(TextId::dlss5_help));
    if (settings.dlss5_before &&
        settings.display_mode == render::DisplayMode::center_remap)
        ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.15f, 1.0f), "%s",
                           t(TextId::dlss5_center_unsupported));

    const std::string diagnostics_label = l(
        TextId::dlss5_diagnostics, "dlss5-diagnostics");
    if (ImGui::CollapsingHeader(diagnostics_label.c_str()))
    {
        const dlss5::HookSnapshot hooks = dlss5::hook_snapshot();
        const dlss5::DiagnosticsSnapshot copies = dlss5::diagnostics_snapshot();
        ImGui::Text("%s: %s", t(TextId::dlss5_module),
                    t(hooks.module_loaded ? TextId::dlss5_loaded
                                          : TextId::dlss5_waiting));
        ImGui::Text("%s: D3D11=%d/%d, D3D12=%d/%d",
                    t(TextId::dlss5_hooks), hooks.d3d11_evaluate,
                    hooks.d3d11_evaluate_c, hooks.d3d12_evaluate,
                    hooks.d3d12_evaluate_c);
        ImGui::Text("%s: D3D11=%llu, D3D12=%llu, pairs=%llu, incomplete=%llu",
                    t(TextId::dlss5_calls),
                    static_cast<unsigned long long>(hooks.d3d11_calls),
                    static_cast<unsigned long long>(hooks.d3d12_calls),
                    static_cast<unsigned long long>(hooks.resource_pairs),
                    static_cast<unsigned long long>(hooks.incomplete_calls));
        ImGui::Text("%s: D3D11=%llu/%llu, D3D12=%llu/%llu",
                    t(TextId::dlss5_copies),
                    static_cast<unsigned long long>(copies.d3d11_applied),
                    static_cast<unsigned long long>(copies.d3d11_skipped),
                    static_cast<unsigned long long>(copies.d3d12_applied),
                    static_cast<unsigned long long>(copies.d3d12_skipped));
        const std::string_view outcome = dlss5::copy_outcome_name(
            copies.last_outcome);
        ImGui::Text("%s: %.*s (%u,%u)-(%u,%u)",
                    t(TextId::dlss5_last_result),
                    static_cast<int>(outcome.size()), outcome.data(),
                    copies.last_region.left, copies.last_region.top,
                    copies.last_region.right, copies.last_region.bottom);
        if (!hooks.last_error.empty())
            ImGui::TextWrapped("%s: %s", t(TextId::dlss5_last_error),
                               hooks.last_error.c_str());
    }
    ImGui::Separator();

    if (settings.display_mode == render::DisplayMode::center_remap)
    {
        numeric_setting(t(TextId::center_focus), "center-focus",
                        settings.center_focus, 0.0f, 1.0f,
                        defaults.center_focus, "%.3f");
        ImGui::TextDisabled("%s", t(TextId::center_sweep_warning));
    }

    const std::string show_border = l(TextId::show_border, "show-border");
    ImGui::Checkbox(show_border.c_str(), &settings.show_border);
    numeric_setting(t(TextId::border_width), "border-width",
                    settings.border_width, 0.0f, 0.02f,
                    defaults.border_width, "%.4f");
    numeric_setting(t(TextId::border_opacity), "border-opacity",
                    settings.border_opacity, 0.0f, 1.0f,
                    defaults.border_opacity, "%.2f");
    const std::string reset = l(TextId::reset_comparison, "reset-comparison");
    if (ImGui::Button(reset.c_str()))
    {
        const bool was_enabled = settings.enabled;
        settings = render::CompositorSettings{};
        settings.enabled = was_enabled;
    }
}

void draw_motion_page()
{
    auto &controller = control::split_motion();
    auto &motion = controller.settings();
    auto &split = render::settings().split_position;
    const control::MotionSettings defaults;

    const std::string frozen = l(TextId::freeze_pair, "freeze-pair");
    ImGui::Checkbox(frozen.c_str(), &motion.frozen);
    numeric_setting(t(TextId::manual_speed), "manual-speed", motion.manual_speed,
                    0.01f, 1.0f, defaults.manual_speed, "%.2f");
    numeric_setting(t(TextId::auto_speed), "auto-speed", motion.auto_speed,
                    0.01f, 1.0f, defaults.auto_speed, "%.2f");

    int mode = static_cast<int>(motion.sweep_mode);
    const std::array<const char *, 3> modes = {
        t(TextId::sweep_left_to_right), t(TextId::sweep_right_to_left),
        t(TextId::sweep_ping_pong)};
    const std::string mode_label = l(TextId::auto_mode, "auto-mode");
    if (ImGui::Combo(mode_label.c_str(), &mode, modes.data(),
                     static_cast<int>(modes.size())))
        motion.sweep_mode = static_cast<control::SweepMode>(mode);

    if (motion.auto_active)
    {
        const std::string stop = l(TextId::stop_and_center, "stop-autosweep");
        if (ImGui::Button(stop.c_str()))
            controller.stop(split);
    }
    else
    {
        const std::string start = l(TextId::start_autosweep, "start-autosweep");
        if (ImGui::Button(start.c_str()))
            controller.start(motion.sweep_mode, split);
    }
    ImGui::SameLine();
    const std::string reset = l(TextId::reset_motion, "reset-motion");
    if (ImGui::Button(reset.c_str()))
        motion = control::MotionSettings{};
}

void draw_hotkeys_page()
{
    auto &keys = input::hotkey_bindings();
    input::draw_binding_editor(t(TextId::move_left), "move-left", keys.move_left);
    input::draw_binding_editor(t(TextId::move_right), "move-right", keys.move_right);
    input::draw_binding_editor(t(TextId::focus_left), "focus-left", keys.focus_left);
    input::draw_binding_editor(t(TextId::focus_right), "focus-right", keys.focus_right);
    input::draw_binding_editor(t(TextId::toggle_comparison), "toggle-comparison",
                               keys.toggle_comparison);
    input::draw_binding_editor(t(TextId::toggle_autosweep), "toggle-autosweep",
                               keys.toggle_auto);
    input::draw_binding_editor(t(TextId::toggle_freeze), "toggle-freeze",
                               keys.toggle_freeze);
    input::draw_binding_editor(t(TextId::toggle_display_mode), "toggle-display-mode",
                               keys.toggle_display_mode);
    input::draw_binding_editor(t(TextId::toggle_border), "toggle-border",
                               keys.toggle_border);
    const std::string reset = l(TextId::reset_hotkeys, "reset-hotkeys");
    if (ImGui::Button(reset.c_str()))
    {
        input::cancel_hotkey_capture();
        keys = input::HotkeyBindings{};
    }
}

void draw_labels_page()
{
    auto &labels = label_settings();
    const LabelSettings defaults;
    const std::string visible = l(TextId::show_labels, "labels-visible");
    ImGui::Checkbox(visible.c_str(), &labels.visible);
    const std::string before = l(TextId::before_text, "before-text");
    const std::string after = l(TextId::after_text, "after-text");
    ImGui::InputText(before.c_str(), labels.before_text.data(), labels.before_text.size());
    ImGui::InputText(after.c_str(), labels.after_text.data(), labels.after_text.size());
    numeric_setting(t(TextId::left_x), "left-x", labels.left_x, 0.0f, 1.0f,
                    defaults.left_x, "%.3f");
    numeric_setting(t(TextId::left_y), "left-y", labels.left_y, 0.0f, 1.0f,
                    defaults.left_y, "%.3f");
    numeric_setting(t(TextId::right_x), "right-x", labels.right_x, 0.0f, 1.0f,
                    defaults.right_x, "%.3f");
    numeric_setting(t(TextId::right_y), "right-y", labels.right_y, 0.0f, 1.0f,
                    defaults.right_y, "%.3f");
    const std::string mirror_right = l(TextId::mirror_left_to_right,
                                       "mirror-left-right");
    if (ImGui::Button(mirror_right.c_str()))
        mirror_left_to_right(labels);
    const std::string mirror_left = l(TextId::mirror_right_to_left,
                                      "mirror-right-left");
    if (ImGui::Button(mirror_left.c_str()))
        mirror_right_to_left(labels);
    numeric_setting(t(TextId::font_size), "font-size", labels.font_size,
                    8.0f, 192.0f, defaults.font_size, "%.0f");
    numeric_setting(t(TextId::text_opacity), "text-opacity", labels.opacity,
                    0.0f, 1.0f, defaults.opacity, "%.2f");
    numeric_setting(t(TextId::outline_width), "outline-width",
                    labels.outline_width, 0.0f, 8.0f,
                    defaults.outline_width, "%.1f");
    numeric_setting(t(TextId::outline_opacity), "outline-opacity",
                    labels.outline_opacity, 0.0f, 1.0f,
                    defaults.outline_opacity, "%.2f");
    const std::string reset = l(TextId::reset_labels, "reset-labels");
    if (ImGui::Button(reset.c_str()))
        labels = LabelSettings{};
}

void draw_status_page()
{
    hud::draw_indicator_panel();
}

void draw_config_page(reshade::api::effect_runtime *runtime)
{
    draw_language_selector("config-language");
    ImGui::TextDisabled("%s", t(TextId::language_help));
    ImGui::Separator();
    ImGui::TextWrapped("%s", t(TextId::config_help));
    const std::string save = l(TextId::save_now, "save-now");
    if (ImGui::Button(save.c_str()))
        config::save_now();
    ImGui::SameLine();
    const std::string reload = l(TextId::reload, "reload-now");
    if (ImGui::Button(reload.c_str()) && config::reload_now())
        capture::reset_runtime_state(runtime);

    const TextId status_id = config_status_text(config::status());
    if (status_id != TextId::count)
        ImGui::TextDisabled("%s", t(status_id));

    const auto *state = capture::state_for(runtime);
    const bool ready = state != nullptr && state->pair.ready();
    ImGui::Separator();
    ImGui::TextUnformatted(t(ready ? TextId::capture_ready
                                   : TextId::capture_waiting));
    ImGui::TextDisabled("%s", t(TextId::vanilla_warning));
}
}
