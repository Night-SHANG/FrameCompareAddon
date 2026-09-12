#pragma once

namespace framecompare::ui
{
bool numeric_setting(const char *label, const char *stable_id, float &value,
                     float minimum, float maximum, float default_value,
                     const char *format);
}
