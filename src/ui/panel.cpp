#include <imgui.h>

#include "ui/panel.hpp"

#include "ui/i18n/localization.hpp"
#include "ui/pages/panel_pages.hpp"

namespace framecompare::ui
{
void draw_panel(reshade::api::effect_runtime *runtime)
{
    pages::draw_language_selector("header-language");
    ImGui::Separator();
    if (!ImGui::BeginTabBar("framecompare-pages",
                            ImGuiTabBarFlags_FittingPolicyScroll))
        return;

    using i18n::TextId;
    const auto tab = [](TextId id, const char *stable_id) {
        return i18n::label(id, stable_id);
    };
    const std::string compare = tab(TextId::tab_compare, "tab-compare");
    if (ImGui::BeginTabItem(compare.c_str()))
    {
        pages::draw_compare_page(runtime);
        ImGui::EndTabItem();
    }
    const std::string motion = tab(TextId::tab_motion, "tab-motion");
    if (ImGui::BeginTabItem(motion.c_str()))
    {
        pages::draw_motion_page();
        ImGui::EndTabItem();
    }
    const std::string hotkeys = tab(TextId::tab_hotkeys, "tab-hotkeys");
    if (ImGui::BeginTabItem(hotkeys.c_str()))
    {
        pages::draw_hotkeys_page();
        ImGui::EndTabItem();
    }
    const std::string labels = tab(TextId::tab_labels, "tab-labels");
    if (ImGui::BeginTabItem(labels.c_str()))
    {
        pages::draw_labels_page();
        ImGui::EndTabItem();
    }
    const std::string status = tab(TextId::tab_status, "tab-status");
    if (ImGui::BeginTabItem(status.c_str()))
    {
        pages::draw_status_page();
        ImGui::EndTabItem();
    }
    const std::string config = tab(TextId::tab_config, "tab-config");
    if (ImGui::BeginTabItem(config.c_str()))
    {
        pages::draw_config_page(runtime);
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}
}
