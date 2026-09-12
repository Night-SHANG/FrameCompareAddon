#include <imgui.h>

#include "ui/panel.hpp"

#include "capture/reshade_capture.hpp"
#include "control/split_motion.hpp"
#include "input/hotkeys.hpp"
#include "render/compositor.hpp"
#include "ui/label_overlay.hpp"
#include "ui/parameter_widgets.hpp"

namespace framecompare::ui
{
void draw_panel(reshade::api::effect_runtime *runtime)
{
    auto &settings = render::settings();
    if (ImGui::Checkbox("启用实时对比 / Enable realtime comparison",
                        &settings.enabled) && !settings.enabled)
    {
        if (auto *state = capture::state_for(runtime))
        {
            state->pair.reset();
            if (state->cycle)
                state->cycle->reset();
            state->before.ready = false;
            state->after.ready = false;
        }
    }
    const render::CompositorSettings compositor_defaults;
    numeric_setting("分割位置 / Split position", settings.split_position,
                    0.0f, 1.0f, compositor_defaults.split_position, "%.3f");
    ImGui::Checkbox("Before 位于左侧 / Before on left",
                    &settings.before_on_left);

    int mode = static_cast<int>(settings.display_mode);
    const char *modes[] = {
        "同坐标擦除 / Same-coordinate wipe",
        "中心偏移 / Center remap"};
    if (ImGui::Combo("显示方式 / Display mode", &mode, modes, 2))
        settings.display_mode = static_cast<render::DisplayMode>(mode);

    ImGui::Checkbox("显示分割线 / Show border", &settings.show_border);
    numeric_setting("线宽 / Border width", settings.border_width,
                    0.0f, 0.02f, compositor_defaults.border_width, "%.4f");
    numeric_setting("线透明度 / Border opacity", settings.border_opacity,
                    0.0f, 1.0f, compositor_defaults.border_opacity, "%.2f");
    if (ImGui::Button("恢复分屏默认 / Reset comparison defaults"))
    {
        const bool enabled = settings.enabled;
        settings = render::CompositorSettings{};
        settings.enabled = enabled;
    }

    auto &motion_controller = control::split_motion();
    auto &motion = motion_controller.settings();
    const control::MotionSettings motion_defaults;
    ImGui::Separator();
    ImGui::TextUnformatted("移动、扫屏与冻结 / Motion, sweep and freeze");
    ImGui::Checkbox("冻结当前画面对 / Freeze current pair", &motion.frozen);
    numeric_setting("手动移动速度 / Manual speed", motion.manual_speed,
                    0.01f, 1.0f, motion_defaults.manual_speed, "%.2f");
    numeric_setting("自动扫屏速度 / Auto speed", motion.auto_speed,
                    0.01f, 1.0f, motion_defaults.auto_speed, "%.2f");
    int sweep_mode = static_cast<int>(motion.sweep_mode);
    const char *sweep_modes[] = {
        "左到右 / Left to right",
        "右到左 / Right to left",
        "往返 / Ping-pong"};
    if (ImGui::Combo("自动模式 / Auto mode", &sweep_mode, sweep_modes, 3))
        motion.sweep_mode = static_cast<control::SweepMode>(sweep_mode);
    if (motion.auto_active)
    {
        if (ImGui::Button("停止自动扫屏 / Stop autosweep"))
            motion_controller.stop();
    }
    else if (ImGui::Button("开始自动扫屏 / Start autosweep"))
    {
        motion_controller.start(motion.sweep_mode, settings.split_position);
    }
    ImGui::SameLine();
    if (ImGui::Button("恢复移动默认 / Reset motion defaults"))
        motion = control::MotionSettings{};

    ImGui::TextUnformatted("快捷键 / Hotkeys");
    auto &bindings = input::hotkey_bindings();
    input::draw_binding_editor("左移 / Move left", bindings.move_left);
    input::draw_binding_editor("右移 / Move right", bindings.move_right);
    input::draw_binding_editor("自动开始/停止 / Toggle autosweep",
                               bindings.toggle_auto);
    input::draw_binding_editor("冻结/继续 / Toggle freeze",
                               bindings.toggle_freeze);
    if (ImGui::Button("恢复默认快捷键 / Reset default hotkeys"))
        bindings = input::HotkeyBindings{};

    auto &labels = label_settings();
    const LabelSettings label_defaults;
    ImGui::Separator();
    ImGui::TextUnformatted("常驻标签 / Persistent labels");
    ImGui::Checkbox("显示标签 / Show labels", &labels.visible);
    ImGui::InputText("Before 文字 / Before text",
                     labels.before_text.data(), labels.before_text.size());
    ImGui::InputText("After 文字 / After text",
                     labels.after_text.data(), labels.after_text.size());
    numeric_setting("左侧 X / Left X", labels.left_x, 0.0f, 1.0f,
                    label_defaults.left_x, "%.3f");
    numeric_setting("左侧 Y / Left Y", labels.left_y, 0.0f, 1.0f,
                    label_defaults.left_y, "%.3f");
    numeric_setting("右侧 X / Right X", labels.right_x, 0.0f, 1.0f,
                    label_defaults.right_x, "%.3f");
    numeric_setting("右侧 Y / Right Y", labels.right_y, 0.0f, 1.0f,
                    label_defaults.right_y, "%.3f");
    if (ImGui::Button("镜像左侧位置到右侧 / Mirror left to right"))
        mirror_left_to_right(labels);
    if (ImGui::Button("镜像右侧位置到左侧 / Mirror right to left"))
        mirror_right_to_left(labels);
    numeric_setting("字号 / Font size", labels.font_size, 8.0f, 192.0f,
                    label_defaults.font_size, "%.0f");
    numeric_setting("文字透明度 / Text opacity", labels.opacity,
                    0.0f, 1.0f, label_defaults.opacity, "%.2f");
    numeric_setting("描边宽度 / Outline width", labels.outline_width,
                    0.0f, 8.0f, label_defaults.outline_width, "%.1f");
    numeric_setting("描边透明度 / Outline opacity", labels.outline_opacity,
                    0.0f, 1.0f, label_defaults.outline_opacity, "%.2f");
    if (ImGui::Button("恢复标签默认 / Reset label defaults"))
        labels = LabelSettings{};

    const auto *state = capture::state_for(runtime);
    const bool ready = state != nullptr && state->pair.ready();
    ImGui::Separator();
    ImGui::TextUnformatted(ready
        ? "捕获：Pre-ReShade FX / Post-ReShade FX（同帧）"
        : "等待同帧画面对 / Waiting for a same-frame pair");
    ImGui::TextDisabled("Pre-ReShade FX 未被认证为 Vanilla。"
                        " / Pre-ReShade FX is not verified Vanilla.");
}
}
