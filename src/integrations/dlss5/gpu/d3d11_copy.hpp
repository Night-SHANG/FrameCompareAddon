#pragma once

#include "integrations/dlss5/model.hpp"

#include <d3d11.h>

namespace framecompare::dlss5
{
CopyOutcome apply_d3d11_copy(ID3D11DeviceContext *context,
                             ID3D11Resource *color,
                             ID3D11Resource *output,
                             const CopyRegion &region) noexcept;
}
