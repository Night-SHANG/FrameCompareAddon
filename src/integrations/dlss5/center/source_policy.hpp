#pragma once

#include <cstdint>

namespace framecompare::dlss5::center
{
enum class BeforeSource : std::uint8_t
{
    generic,
    dlss_input,
};

struct SourceConditions
{
    bool dlss5_enabled = false;
    bool center_remap = false;
    bool bridge_ready = false;
    std::uint64_t available_generation = 0;
    std::uint64_t consumed_generation = 0;
};

BeforeSource select_source(SourceConditions conditions) noexcept;
}

