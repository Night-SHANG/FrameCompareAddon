#pragma once

namespace framecompare::ui
{
bool numeric_setting(const char *label, float &value,
                     float minimum, float maximum, float default_value,
                     const char *format);
}
