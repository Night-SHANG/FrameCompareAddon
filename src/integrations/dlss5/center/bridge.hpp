#pragma once

#include "integrations/dlss5/runtime.hpp"

#include <d3d11.h>
#include <d3d12.h>
#include <reshade.hpp>

#include <cstdint>
#include <string>

namespace framecompare::dlss5::center
{
class SourceLease
{
public:
    SourceLease() = default;
    SourceLease(const SourceLease &) = delete;
    SourceLease &operator=(const SourceLease &) = delete;
    SourceLease(SourceLease &&other) noexcept;
    SourceLease &operator=(SourceLease &&other) noexcept;
    ~SourceLease();

    explicit operator bool() const noexcept;
    reshade::api::resource resource() const noexcept;
    std::uint64_t generation() const noexcept;

private:
    friend SourceLease acquire_latest(reshade::api::effect_runtime *,
                                      std::uint64_t);
    SourceLease(ID3D11Texture2D *texture, std::uint64_t generation) noexcept;

    ID3D11Texture2D *texture_ = nullptr;
    std::uint64_t generation_ = 0;
};

struct BridgeSnapshot
{
    bool runtime_attached = false;
    bool ready = false;
    bool cross_api = false;
    std::uint64_t generation = 0;
    Api last_api = Api::none;
    CopyOutcome last_outcome = CopyOutcome::none;
    std::string last_error;
};

void attach_runtime(reshade::api::effect_runtime *runtime) noexcept;
void detach_runtime(reshade::api::effect_runtime *runtime) noexcept;
void shutdown() noexcept;

CopyOutcome capture_d3d11(ID3D11DeviceContext *context,
                          ID3D11Resource *color) noexcept;
CopyOutcome capture_d3d12(ID3D12GraphicsCommandList *commands,
                          ID3D12Resource *color) noexcept;
SourceLease acquire_latest(reshade::api::effect_runtime *runtime,
                           std::uint64_t consumed_generation);
void discard_pending(reshade::api::effect_runtime *runtime = nullptr) noexcept;
BridgeSnapshot bridge_snapshot();
}
