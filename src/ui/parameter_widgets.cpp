#include <imgui.h>
#include <reshade.hpp>

#include "ui/parameter_widgets.hpp"
#include "ui/i18n/localization.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace framecompare::ui
{
bool numeric_setting(const char *label, const char *stable_id, float &value,
                     float minimum, float maximum, float default_value,
                     const char *format)
{
    bool changed = false;
    ImGui::TextUnformatted(label);

    const std::string slider_id = std::string("##slider-") + stable_id;
    ImGui::SetNextItemWidth(180.0f);
    changed |= ImGui::SliderFloat(slider_id.c_str(), &value,
                                  minimum, maximum, format);

    ImGui::SameLine();
    const std::string input_id = std::string("##number-") + stable_id;
    ImGui::SetNextItemWidth(100.0f);
    changed |= ImGui::InputFloat(input_id.c_str(), &value, 0.0f, 0.0f,
                                 format, ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::SameLine();
    const std::string reset_id = i18n::label(i18n::TextId::reset,
                                             std::string("reset-") + stable_id);
    if (ImGui::Button(reset_id.c_str()))
    {
        value = default_value;
        changed = true;
    }

    if (!std::isfinite(value))
        value = default_value;
    value = std::clamp(value, minimum, maximum);
    return changed;
}
}
