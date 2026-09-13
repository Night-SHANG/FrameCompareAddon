#pragma once

#include "integrations/dlss5/model.hpp"

#include <cstdint>

namespace framecompare::dlss5
{
enum class Api : std::uint8_t
{
    none,
    d3d11,
    d3d12,
};

enum class Operation : std::uint8_t
{
    disabled,
    same_coordinate_region,
    center_full_frame,
};

struct SettingsSnapshot
{
    Operation operation = Operation::disabled;
    float split_position = 0.5f;
    bool before_on_left = true;
};

struct DiagnosticsSnapshot
{
    std::uint64_t d3d11_applied = 0;
    std::uint64_t d3d11_skipped = 0;
    std::uint64_t d3d12_applied = 0;
    std::uint64_t d3d12_skipped = 0;
    Api last_api = Api::none;
    CopyOutcome last_outcome = CopyOutcome::none;
    CopyRegion last_region{};
};

void publish_settings(SettingsSnapshot settings) noexcept;
SettingsSnapshot settings_snapshot() noexcept;
void record_result(Api api, CopyOutcome outcome, CopyRegion region) noexcept;
DiagnosticsSnapshot diagnostics_snapshot() noexcept;
}
