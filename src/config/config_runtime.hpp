#pragma once

#include <string>

namespace framecompare::config
{
void initialize(void *addon_module);
void shutdown();
void tick();
bool save_now();
bool reload_now();
const std::string &status_message() noexcept;
}
