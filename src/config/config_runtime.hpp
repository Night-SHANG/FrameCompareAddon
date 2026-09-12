#pragma once

namespace framecompare::config
{
enum class ConfigStatus
{
    none,
    save_failed,
    saved,
    reload_failed,
    reloaded
};

void initialize(void *addon_module);
void shutdown();
void tick();
bool save_now();
bool reload_now();
ConfigStatus status() noexcept;
}
