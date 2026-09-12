#include <imgui.h>
#include <reshade.hpp>

#include "hud/indicator_panel.hpp"

#include "hud/indicator_model.hpp"
#include "input/hotkeys.hpp"
#include "ui/parameter_widgets.hpp"

#include <string>

namespace framecompare::hud
{
void draw_indicator_panel()
{
    ImGui::Separator();
    ImGui::TextUnformatted("自定义状态提示 / Custom status indicators");
    ImGui::TextWrapped(
        "ReShade 状态直接读取真实效果开关；快捷键跟踪只根据相同按键"
        "翻转，不能验证外部插件的真实状态。 / ReShade state is read "
        "directly; tracked hotkeys cannot verify another add-on's state.");

    auto &items = indicators();
    if (ImGui::Button("新增状态条目 / Add status item") &&
        items.size() < maximum_indicators)
    {
        Indicator item;
        item.y = 0.12f + 0.05f * static_cast<float>(items.size() % 10);
        items.push_back(item);
    }

    for (std::size_t index = 0; index < items.size();)
    {
        Indicator &item = items[index];
        ImGui::PushID(static_cast<int>(index));
        const std::string title = std::string(item.name.data()) +
                                  "###status-item";
        const bool open = ImGui::TreeNodeEx(
            title.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::SameLine();
        if (ImGui::SmallButton("移除 / Remove"))
        {
            input::cancel_hotkey_capture();
            items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));
            ImGui::PopID();
            continue;
        }

        if (open)
        {
            ImGui::Checkbox("启用 / Enabled", &item.enabled);
            ImGui::InputText("名称 / Name", item.name.data(),
                             item.name.size());

            int source = static_cast<int>(item.source);
            const char *sources[] = {
                "快捷键跟踪状态 / Tracked hotkey state",
                "ReShade 实际效果状态 / Actual ReShade effects state"};
            if (ImGui::Combo("状态来源 / State source", &source, sources, 2))
            {
                input::cancel_hotkey_capture();
                item.source = static_cast<IndicatorSource>(source);
                reset_indicator_runtime(item);
            }

            if (item.source == IndicatorSource::tracked_hotkey)
            {
                ImGuiKeyChord chord =
                    static_cast<ImGuiKeyChord>(item.hotkey_chord);
                const std::string label = "快捷键 / Hotkey #" +
                                          std::to_string(index + 1);
                if (input::draw_binding_editor(label.c_str(), chord))
                {
                    item.hotkey_chord = static_cast<std::uint32_t>(chord);
                    reset_indicator_runtime(item);
                }
                if (ImGui::Checkbox("初始状态为 ON / Initial state ON",
                                    &item.initial_on))
                    reset_indicator_runtime(item);
            }
            else
            {
                ImGui::TextDisabled(
                    "直接读取 ReShade 当前状态，无需绑定 END。"
                    " / Reads ReShade directly; no END binding needed.");
            }

            ImGui::InputText("ON 文字 / ON text", item.text_on.data(),
                             item.text_on.size());
            ImGui::InputText("OFF 文字 / OFF text", item.text_off.data(),
                             item.text_off.size());
            const Indicator defaults;
            ui::numeric_setting("X", item.x, 0.0f, 1.0f,
                                defaults.x, "%.3f");
            ui::numeric_setting("Y", item.y, 0.0f, 1.0f,
                                defaults.y, "%.3f");
            ui::numeric_setting(
                "显示秒数（0=常驻） / Seconds (0=persistent)",
                item.show_seconds, 0.0f, 60.0f,
                defaults.show_seconds, "%.1f");
            ImGui::TextDisabled("当前状态 / Current state: %s",
                                item.runtime_on ? "ON" : "OFF");
            ImGui::TreePop();
        }
        ImGui::PopID();
        ++index;
    }
}
}
