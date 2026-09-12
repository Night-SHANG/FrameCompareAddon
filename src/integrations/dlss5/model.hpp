#pragma once

#include <cstdint>
#include <string_view>

namespace framecompare::dlss5
{
struct TextureShape
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t format = 0;
    std::uint32_t mip_levels = 0;
    std::uint32_t array_size = 0;
    std::uint32_t sample_count = 0;
    std::uint32_t sample_quality = 0;
};

struct CopyRegion
{
    std::uint32_t left = 0;
    std::uint32_t top = 0;
    std::uint32_t right = 0;
    std::uint32_t bottom = 0;

    bool empty() const noexcept;
};

enum class Compatibility : std::uint8_t
{
    compatible,
    invalid_dimensions,
    dimensions_mismatch,
    format_mismatch,
    subresource_mismatch,
    multisampled,
};

enum class CopyOutcome : std::uint8_t
{
    none,
    applied,
    disabled,
    incomplete_parameters,
    incompatible,
    empty_region,
    device_mismatch,
    guarded_failure,
};

Compatibility validate_copy(const TextureShape &source,
                            const TextureShape &destination) noexcept;
CopyRegion make_copy_region(std::uint32_t width, std::uint32_t height,
                            float split_position,
                            bool before_on_left) noexcept;
std::string_view copy_outcome_name(CopyOutcome outcome) noexcept;
}
