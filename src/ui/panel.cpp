#include "ui/panel.hpp"

#include "capture/reshade_capture.hpp"
#include "render/compositor.hpp"

#include <imgui.h>

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
