#include <imgui.h>
#include <reshade.hpp>

#include "hud/indicator_panel.hpp"

#include "hud/indicator_model.hpp"
#include "input/hotkeys.hpp"
#include "ui/i18n/localization.hpp"
#include "ui/parameter_widgets.hpp"

#include <string>

namespace framecompare::hud
{
void draw_indicator_panel()
{
    using ui::i18n::TextId;
    const auto t = [](TextId id) { return ui::i18n::text(id); };
    const auto l = [](TextId id, const char *stable_id) {
        return ui::i18n::label(id, stable_id);
    };
    ImGui::TextWrapped("%s", t(TextId::status_help));

    auto &items = indicators();
    const std::string add = l(TextId::add_status_item, "add-status-item");
    if (ImGui::Button(add.c_str()) &&
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
        const std::string remove = l(TextId::remove, "remove-status-item");
        if (ImGui::SmallButton(remove.c_str()))
        {
            input::cancel_hotkey_capture();
            items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));
            ImGui::PopID();
            continue;
        }

        if (open)
        {
            const std::string enabled = l(TextId::enabled, "status-enabled");
            const std::string name = l(TextId::name, "status-name");
            ImGui::Checkbox(enabled.c_str(), &item.enabled);
            ImGui::InputText(name.c_str(), item.name.data(),
                             item.name.size());

            int source = static_cast<int>(item.source);
            const char *sources[] = {
                t(TextId::source_tracked_hotkey), t(TextId::source_reshade)};
            const std::string source_label = l(TextId::state_source,
                                                "status-source");
            if (ImGui::Combo(source_label.c_str(), &source, sources, 2))
            {
                input::cancel_hotkey_capture();
                item.source = static_cast<IndicatorSource>(source);
                reset_indicator_runtime(item);
            }

            if (item.source == IndicatorSource::tracked_hotkey)
            {
                ImGuiKeyChord chord =
                    static_cast<ImGuiKeyChord>(item.hotkey_chord);
                const std::string stable_id = "status-hotkey-" +
                                              std::to_string(index);
                if (input::draw_binding_editor(t(TextId::hotkey),
                                               stable_id.c_str(), chord))
                {
                    item.hotkey_chord = static_cast<std::uint32_t>(chord);
                    reset_indicator_runtime(item);
                }
                const std::string initial = l(TextId::initial_state_on,
                                              "status-initial-on");
                if (ImGui::Checkbox(initial.c_str(), &item.initial_on))
                    reset_indicator_runtime(item);
            }
            else
            {
                ImGui::TextDisabled("%s", t(TextId::reshade_source_help));
            }

            const std::string on_text = l(TextId::on_text, "status-on-text");
            const std::string off_text = l(TextId::off_text, "status-off-text");
            ImGui::InputText(on_text.c_str(), item.text_on.data(),
                             item.text_on.size());
            ImGui::InputText(off_text.c_str(), item.text_off.data(),
                             item.text_off.size());
            const Indicator defaults;
            ui::numeric_setting(t(TextId::x), "status-x", item.x, 0.0f, 1.0f,
                                defaults.x, "%.3f");
            ui::numeric_setting(t(TextId::y), "status-y", item.y, 0.0f, 1.0f,
                                defaults.y, "%.3f");
            ui::numeric_setting(
                t(TextId::show_seconds), "status-show-seconds",
                item.show_seconds, 0.0f, 60.0f,
                defaults.show_seconds, "%.1f");
            ImGui::TextDisabled("%s: %s", t(TextId::current_state),
                                item.runtime_on ? "ON" : "OFF");
            ImGui::TreePop();
        }
        ImGui::PopID();
        ++index;
    }
}
}
