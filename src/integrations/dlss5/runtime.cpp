#include "integrations/dlss5/runtime.hpp"

#include <atomic>
#include <cstring>
#include <mutex>

namespace framecompare::dlss5
{
namespace
{
std::atomic<std::uint64_t> g_settings{
    (static_cast<std::uint64_t>(0x3F000000u) << 2u) | 0x2u};
std::mutex g_diagnostics_mutex;
DiagnosticsSnapshot g_diagnostics;

std::uint32_t float_bits(float value) noexcept
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

float bits_float(std::uint32_t bits) noexcept
{
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
}

void publish_settings(SettingsSnapshot settings) noexcept
{
    const std::uint64_t packed =
        (static_cast<std::uint64_t>(float_bits(settings.split_position)) << 2u) |
        (settings.enabled ? 0x1u : 0u) |
        (settings.before_on_left ? 0x2u : 0u);
    g_settings.store(packed, std::memory_order_release);
}

SettingsSnapshot settings_snapshot() noexcept
{
    const std::uint64_t packed = g_settings.load(std::memory_order_acquire);
    SettingsSnapshot result;
    result.enabled = (packed & 0x1u) != 0;
    result.before_on_left = (packed & 0x2u) != 0;
    result.split_position = bits_float(static_cast<std::uint32_t>(packed >> 2u));
    return result;
}

void record_result(Api api, CopyOutcome outcome, CopyRegion region) noexcept
{
    std::lock_guard lock(g_diagnostics_mutex);
    auto &applied = api == Api::d3d11 ? g_diagnostics.d3d11_applied
                                      : g_diagnostics.d3d12_applied;
    auto &skipped = api == Api::d3d11 ? g_diagnostics.d3d11_skipped
                                      : g_diagnostics.d3d12_skipped;
    if (api != Api::none)
    {
        if (outcome == CopyOutcome::applied)
            ++applied;
        else
            ++skipped;
    }
    g_diagnostics.last_api = api;
    g_diagnostics.last_outcome = outcome;
    g_diagnostics.last_region = region;
}

DiagnosticsSnapshot diagnostics_snapshot() noexcept
{
    std::lock_guard lock(g_diagnostics_mutex);
    return g_diagnostics;
}
}
