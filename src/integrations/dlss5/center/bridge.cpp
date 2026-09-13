#include "integrations/dlss5/center/bridge.hpp"

#include "integrations/dlss5/model.hpp"

#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <limits>
#include <mutex>
#include <utility>

namespace framecompare::dlss5::center
{
namespace
{
using Microsoft::WRL::ComPtr;

struct BridgeState
{
    reshade::api::effect_runtime *runtime = nullptr;
    ComPtr<ID3D11Device> device11;
    ComPtr<ID3D11Texture2D> texture11;
    ComPtr<ID3D12Device> device12;
    ComPtr<ID3D12Resource> texture12;
    TextureShape shape{};
    std::uint64_t generation = 0;
    Api last_api = Api::none;
    CopyOutcome last_outcome = CopyOutcome::none;
    bool cross_api = false;
    std::string last_error;
};

std::mutex g_mutex;
BridgeState g_state;

TextureShape shape(const D3D11_TEXTURE2D_DESC &desc) noexcept
{
    return {desc.Width, desc.Height, static_cast<std::uint32_t>(desc.Format),
            desc.MipLevels, desc.ArraySize, desc.SampleDesc.Count,
            desc.SampleDesc.Quality};
}

TextureShape shape(const D3D12_RESOURCE_DESC &desc) noexcept
{
    return {static_cast<std::uint32_t>(desc.Width), desc.Height,
            static_cast<std::uint32_t>(desc.Format), desc.MipLevels,
            desc.DepthOrArraySize, desc.SampleDesc.Count,
            desc.SampleDesc.Quality};
}

void clear_textures() noexcept
{
    g_state.texture11.Reset();
    g_state.texture12.Reset();
    g_state.device12.Reset();
    g_state.shape = {};
    g_state.cross_api = false;
}

void result(Api api, CopyOutcome outcome, const char *error = nullptr)
{
    g_state.last_api = api;
    g_state.last_outcome = outcome;
    try { g_state.last_error = error != nullptr ? error : ""; }
    catch (...) { g_state.last_error.clear(); }
}

bool same_adapter(ID3D11Device *device11, ID3D12Device *device12) noexcept
{
    ComPtr<IDXGIDevice> dxgi_device;
    ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC desc{};
    if (FAILED(device11->QueryInterface(IID_PPV_ARGS(&dxgi_device))) ||
        FAILED(dxgi_device->GetAdapter(&adapter)) ||
        FAILED(adapter->GetDesc(&desc)))
        return false;
    const LUID other = device12->GetAdapterLuid();
    return desc.AdapterLuid.HighPart == other.HighPart &&
           desc.AdapterLuid.LowPart == other.LowPart;
}

D3D12_RESOURCE_BARRIER transition(ID3D12Resource *resource,
                                  D3D12_RESOURCE_STATES before,
                                  D3D12_RESOURCE_STATES after) noexcept
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    return barrier;
}

bool create_shared_pair(ID3D12Device *device12,
                        const D3D12_RESOURCE_DESC &source_desc)
{
    clear_textures();
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc = source_desc;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
    HRESULT hr = device12->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_SHARED, &desc, D3D12_RESOURCE_STATE_COMMON,
        nullptr, IID_PPV_ARGS(&g_state.texture12));
    HANDLE shared = nullptr;
    if (SUCCEEDED(hr))
        hr = device12->CreateSharedHandle(g_state.texture12.Get(), nullptr,
                                          GENERIC_ALL, nullptr, &shared);
    ComPtr<ID3D11Device1> device11_1;
    if (SUCCEEDED(hr)) hr = g_state.device11.As(&device11_1);
    if (SUCCEEDED(hr))
        hr = device11_1->OpenSharedResource1(
            shared, IID_PPV_ARGS(&g_state.texture11));
    if (shared != nullptr) CloseHandle(shared);
    if (FAILED(hr))
    {
        g_state.texture11.Reset();
        g_state.texture12.Reset();
        D3D11_TEXTURE2D_DESC desc11{};
        desc11.Width = static_cast<UINT>(source_desc.Width);
        desc11.Height = source_desc.Height;
        desc11.MipLevels = source_desc.MipLevels;
        desc11.ArraySize = source_desc.DepthOrArraySize;
        desc11.Format = source_desc.Format;
        desc11.SampleDesc = source_desc.SampleDesc;
        desc11.Usage = D3D11_USAGE_DEFAULT;
        desc11.BindFlags = D3D11_BIND_SHADER_RESOURCE |
                           D3D11_BIND_RENDER_TARGET;
        desc11.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                           D3D11_RESOURCE_MISC_SHARED;
        hr = g_state.device11->CreateTexture2D(
            &desc11, nullptr, &g_state.texture11);
        ComPtr<IDXGIResource1> dxgi_resource;
        shared = nullptr;
        if (SUCCEEDED(hr)) hr = g_state.texture11.As(&dxgi_resource);
        if (SUCCEEDED(hr))
            hr = dxgi_resource->CreateSharedHandle(
                nullptr, DXGI_SHARED_RESOURCE_READ |
                             DXGI_SHARED_RESOURCE_WRITE,
                nullptr, &shared);
        if (SUCCEEDED(hr))
            hr = device12->OpenSharedHandle(
                shared, IID_PPV_ARGS(&g_state.texture12));
        if (shared != nullptr) CloseHandle(shared);
    }
    if (FAILED(hr))
    {
        clear_textures();
        return false;
    }
    g_state.device12 = device12;
    g_state.shape = shape(source_desc);
    g_state.cross_api = true;
    return true;
}

CopyOutcome capture11_impl(ID3D11DeviceContext *context,
                           ID3D11Resource *color)
{
    if (context == nullptr || color == nullptr)
        return CopyOutcome::incomplete_parameters;
    ComPtr<ID3D11Texture2D> source;
    if (FAILED(color->QueryInterface(IID_PPV_ARGS(&source))))
        return CopyOutcome::incompatible;
    D3D11_TEXTURE2D_DESC desc{};
    source->GetDesc(&desc);
    const TextureShape source_shape = shape(desc);
    if (validate_copy(source_shape, source_shape) != Compatibility::compatible ||
        desc.MipLevels != 1 || desc.ArraySize != 1)
        return CopyOutcome::incompatible;
    ComPtr<ID3D11Device> context_device;
    context->GetDevice(&context_device);
    if (!g_state.device11 || context_device.Get() != g_state.device11.Get())
        return CopyOutcome::device_mismatch;
    if (!g_state.texture11 || g_state.cross_api ||
        validate_copy(g_state.shape, source_shape) != Compatibility::compatible)
    {
        clear_textures();
        D3D11_TEXTURE2D_DESC capture_desc = desc;
        capture_desc.Usage = D3D11_USAGE_DEFAULT;
        capture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        capture_desc.CPUAccessFlags = 0;
        capture_desc.MiscFlags = 0;
        if (FAILED(g_state.device11->CreateTexture2D(
                &capture_desc, nullptr, &g_state.texture11)))
            return CopyOutcome::incompatible;
        g_state.shape = source_shape;
    }
    context->CopyResource(g_state.texture11.Get(), source.Get());
    ++g_state.generation;
    g_state.cross_api = false;
    return CopyOutcome::applied;
}

CopyOutcome capture12_impl(ID3D12GraphicsCommandList *commands,
                           ID3D12Resource *color)
{
    if (commands == nullptr || color == nullptr || !g_state.device11)
        return CopyOutcome::incomplete_parameters;
    const D3D12_RESOURCE_DESC desc = color->GetDesc();
    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
        desc.Width > std::numeric_limits<std::uint32_t>::max() ||
        desc.MipLevels != 1 || desc.DepthOrArraySize != 1 ||
        validate_copy(shape(desc), shape(desc)) != Compatibility::compatible)
        return CopyOutcome::incompatible;
    ComPtr<ID3D12Device> device12;
    if (FAILED(commands->GetDevice(IID_PPV_ARGS(&device12))) ||
        !same_adapter(g_state.device11.Get(), device12.Get()))
        return CopyOutcome::device_mismatch;
    const TextureShape source_shape = shape(desc);
    if (!g_state.texture12 || !g_state.cross_api ||
        g_state.device12.Get() != device12.Get() ||
        validate_copy(g_state.shape, source_shape) != Compatibility::compatible)
    {
        if (!create_shared_pair(device12.Get(), desc))
            return CopyOutcome::incompatible;
    }
    D3D12_RESOURCE_BARRIER barriers[2] = {
        transition(color, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_COPY_SOURCE),
        transition(g_state.texture12.Get(), D3D12_RESOURCE_STATE_COMMON,
                   D3D12_RESOURCE_STATE_COPY_DEST)};
    commands->ResourceBarrier(2, barriers);
    commands->CopyResource(g_state.texture12.Get(), color);
    barriers[0] = transition(color, D3D12_RESOURCE_STATE_COPY_SOURCE,
                             D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    barriers[1] = transition(g_state.texture12.Get(),
                             D3D12_RESOURCE_STATE_COPY_DEST,
                             D3D12_RESOURCE_STATE_COMMON);
    commands->ResourceBarrier(2, barriers);
    ++g_state.generation;
    return CopyOutcome::applied;
}

CopyOutcome guarded_capture11(ID3D11DeviceContext *context,
                              ID3D11Resource *color) noexcept
{
#if defined(_MSC_VER)
    __try { return capture11_impl(context, color); }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return CopyOutcome::guarded_failure;
    }
#else
    return capture11_impl(context, color);
#endif
}

CopyOutcome guarded_capture12(ID3D12GraphicsCommandList *commands,
                              ID3D12Resource *color) noexcept
{
#if defined(_MSC_VER)
    __try { return capture12_impl(commands, color); }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return CopyOutcome::guarded_failure;
    }
#else
    return capture12_impl(commands, color);
#endif
}
}

SourceLease::SourceLease(ID3D11Texture2D *texture,
                         std::uint64_t generation) noexcept
    : texture_(texture), generation_(generation)
{
    if (texture_ != nullptr) texture_->AddRef();
}

SourceLease::SourceLease(SourceLease &&other) noexcept
    : texture_(std::exchange(other.texture_, nullptr)),
      generation_(std::exchange(other.generation_, 0)) {}

SourceLease &SourceLease::operator=(SourceLease &&other) noexcept
{
    if (this != &other)
    {
        if (texture_ != nullptr) texture_->Release();
        texture_ = std::exchange(other.texture_, nullptr);
        generation_ = std::exchange(other.generation_, 0);
    }
    return *this;
}

SourceLease::~SourceLease()
{
    if (texture_ != nullptr) texture_->Release();
}

SourceLease::operator bool() const noexcept { return texture_ != nullptr; }

reshade::api::resource SourceLease::resource() const noexcept
{
    return {reinterpret_cast<std::uintptr_t>(texture_)};
}

std::uint64_t SourceLease::generation() const noexcept { return generation_; }

void attach_runtime(reshade::api::effect_runtime *runtime) noexcept
{
    if (runtime == nullptr || runtime->get_device()->get_api() !=
            reshade::api::device_api::d3d11)
        return;
    std::lock_guard lock(g_mutex);
    auto *device = reinterpret_cast<ID3D11Device *>(
        runtime->get_device()->get_native());
    if (device == nullptr) return;
    if (g_state.runtime != runtime || g_state.device11.Get() != device)
    {
        clear_textures();
        g_state.device11 = device;
        g_state.runtime = runtime;
        g_state.generation = 0;
        g_state.last_error.clear();
    }
}

void detach_runtime(reshade::api::effect_runtime *runtime) noexcept
{
    std::lock_guard lock(g_mutex);
    if (g_state.runtime != runtime) return;
    clear_textures();
    g_state.device11.Reset();
    g_state.runtime = nullptr;
}

void shutdown() noexcept
{
    std::lock_guard lock(g_mutex);
    clear_textures();
    g_state.device11.Reset();
    g_state.runtime = nullptr;
}

CopyOutcome capture_d3d11(ID3D11DeviceContext *context,
                          ID3D11Resource *color) noexcept
{
    std::lock_guard lock(g_mutex);
    const CopyOutcome outcome = guarded_capture11(context, color);
    result(Api::d3d11, outcome,
           outcome == CopyOutcome::applied ? nullptr : "D3D11 full capture failed");
    return outcome;
}

CopyOutcome capture_d3d12(ID3D12GraphicsCommandList *commands,
                          ID3D12Resource *color) noexcept
{
    std::lock_guard lock(g_mutex);
    const CopyOutcome outcome = guarded_capture12(commands, color);
    result(Api::d3d12, outcome,
           outcome == CopyOutcome::applied ? nullptr : "D3D12 shared capture failed");
    return outcome;
}

SourceLease acquire_latest(reshade::api::effect_runtime *runtime,
                           std::uint64_t consumed_generation)
{
    std::lock_guard lock(g_mutex);
    if (runtime != g_state.runtime || !g_state.texture11 ||
        g_state.generation == 0 || g_state.generation <= consumed_generation)
        return {};
    return SourceLease(g_state.texture11.Get(), g_state.generation);
}

BridgeSnapshot bridge_snapshot()
{
    std::lock_guard lock(g_mutex);
    return {g_state.runtime != nullptr, g_state.texture11 != nullptr,
            g_state.cross_api, g_state.generation, g_state.last_api,
            g_state.last_outcome, g_state.last_error};
}
}
