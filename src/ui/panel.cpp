#include <imgui.h>

#include "ui/panel.hpp"

#include "capture/reshade_capture.hpp"
#include "control/split_motion.hpp"
#include "input/hotkeys.hpp"
#include "render/compositor.hpp"
#include "ui/label_overlay.hpp"

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
    ImGui::SliderFloat("分割位置 / Split position",
                       &settings.split_position, 0.0f, 1.0f, "%.3f");
    ImGui::Checkbox("Before 位于左侧 / Before on left",
                    &settings.before_on_left);

    int mode = static_cast<int>(settings.display_mode);
    const char *modes[] = {
        "同坐标擦除 / Same-coordinate wipe",
        "中心偏移 / Center remap"};
    if (ImGui::Combo("显示方式 / Display mode", &mode, modes, 2))
        settings.display_mode = static_cast<render::DisplayMode>(mode);

    ImGui::Checkbox("显示分割线 / Show border", &settings.show_border);
    ImGui::SliderFloat("线宽 / Border width", &settings.border_width,
                       0.0f, 0.02f, "%.4f");
    ImGui::SliderFloat("线透明度 / Border opacity",
                       &settings.border_opacity, 0.0f, 1.0f, "%.2f");

    auto &motion_controller = control::split_motion();
    auto &motion = motion_controller.settings();
    ImGui::Separator();
    ImGui::TextUnformatted("移动、扫屏与冻结 / Motion, sweep and freeze");
    ImGui::Checkbox("冻结当前画面对 / Freeze current pair", &motion.frozen);
    ImGui::SliderFloat("手动移动速度 / Manual speed", &motion.manual_speed,
                       0.01f, 1.0f, "%.2f screen/s");
    ImGui::SliderFloat("自动扫屏速度 / Auto speed", &motion.auto_speed,
                       0.01f, 1.0f, "%.2f screen/s");
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

    ImGui::TextUnformatted("快捷键 / Hotkeys");
    auto &bindings = input::hotkey_bindings();
    input::draw_binding_editor("左移 / Move left", bindings.move_left);
    input::draw_binding_editor("右移 / Move right", bindings.move_right);
    input::draw_binding_editor("自动开始/停止 / Toggle autosweep",
                               bindings.toggle_auto);
    input::draw_binding_editor("冻结/继续 / Toggle freeze",
                               bindings.toggle_freeze);

    auto &labels = label_settings();
    ImGui::Separator();
    ImGui::TextUnformatted("常驻标签 / Persistent labels");
    ImGui::Checkbox("显示标签 / Show labels", &labels.visible);
    ImGui::InputText("Before 文字 / Before text",
                     labels.before_text.data(), labels.before_text.size());
    ImGui::InputText("After 文字 / After text",
                     labels.after_text.data(), labels.after_text.size());
    ImGui::SliderFloat("左侧 X / Left X", &labels.left_x,
                       0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("左侧 Y / Left Y", &labels.left_y,
                       0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("右侧 X / Right X", &labels.right_x,
                       0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("右侧 Y / Right Y", &labels.right_y,
                       0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("字号 / Font size", &labels.font_size,
                       8.0f, 192.0f, "%.0f");
    ImGui::SliderFloat("文字透明度 / Text opacity", &labels.opacity,
                       0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("描边宽度 / Outline width", &labels.outline_width,
                       0.0f, 8.0f, "%.1f");
    ImGui::SliderFloat("描边透明度 / Outline opacity",
                       &labels.outline_opacity, 0.0f, 1.0f, "%.2f");

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
