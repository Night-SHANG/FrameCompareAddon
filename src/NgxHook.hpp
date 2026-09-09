#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace framecompare::ngx
{
    enum class snapshot_api : uint32_t
    {
        none = 0,
        d3d11,
        d3d12,
    };

    struct native_capture
    {
        snapshot_api api = snapshot_api::none;
        Microsoft::WRL::ComPtr<ID3D11Device> device11;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture11;
        Microsoft::WRL::ComPtr<ID3D12Device> device12;
        Microsoft::WRL::ComPtr<ID3D12Resource> texture12;
        HANDLE shared_handle = nullptr;
        D3D11_TEXTURE2D_DESC desc11 = {};
        D3D12_RESOURCE_DESC desc12 = {};
        bool cross_device_shareable = false;

        ~native_capture();
    };

    struct snapshot
    {
        std::shared_ptr<native_capture> native;
        uint64_t generation = 0;
        std::chrono::steady_clock::time_point captured_at = {};
        std::wstring hook_module;
    };

    struct diagnostics
    {
        bool initialized = false;
        bool capture_enabled = false;
        bool create11_hooked = false;
        bool create12_hooked = false;
        bool eval11_hooked = false;
        bool eval11_c_hooked = false;
        bool eval12_hooked = false;
        bool eval12_c_hooked = false;
        bool release11_hooked = false;
        bool release12_hooked = false;
        uint64_t feature18_creates = 0;
        uint64_t feature18_evaluates = 0;
        uint64_t unknown_handle_evaluates = 0;
        uint64_t successful_captures = 0;
        uint64_t failed_captures = 0;
        uint64_t latest_generation = 0;
        snapshot_api latest_api = snapshot_api::none;
        uint32_t latest_width = 0;
        uint32_t latest_height = 0;
        std::wstring hook_module;
        std::string last_error;
    };

    bool initialize();
    void shutdown();

    void set_capture_enabled(bool enabled);
    void set_d3d12_color_state(D3D12_RESOURCE_STATES state);

    std::shared_ptr<const snapshot> latest_snapshot();
    diagnostics get_diagnostics();
}
