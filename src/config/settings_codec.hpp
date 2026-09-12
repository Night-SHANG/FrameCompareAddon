#pragma once

#include <string>
#include <string_view>

namespace framecompare::config
{
std::string encode_current_settings();
void decode_current_settings(std::string_view text);
}
