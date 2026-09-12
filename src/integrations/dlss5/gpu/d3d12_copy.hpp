#pragma once

#include "integrations/dlss5/model.hpp"

#include <d3d12.h>

namespace framecompare::dlss5
{
CopyOutcome apply_d3d12_copy(ID3D12GraphicsCommandList *commands,
                             ID3D12Resource *color,
                             ID3D12Resource *output,
                             const CopyRegion &region) noexcept;
}
