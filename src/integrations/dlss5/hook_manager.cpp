#include "integrations/dlss5/hook_manager.hpp"

#include "integrations/dlss5/gpu/d3d11_copy.hpp"
#include "integrations/dlss5/gpu/d3d12_copy.hpp"
#include "integrations/dlss5/ngx_abi.hpp"
#include "integrations/dlss5/runtime.hpp"

#include <MinHook.h>
#include <reshade.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cwctype>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace framecompare::dlss5
{
namespace
{
struct EvaluationInfo
{
    void *color = nullptr;
    void *output = nullptr;
    std::uint64_t width = 0;
    std::uint32_t height = 0;
    const char *color_key = "none";
    const char *output_key = "none";
};

enum class HookFlag { d3d11, d3d11c, d3d12, d3d12c };

std::atomic_bool g_running{false};
std::thread g_scanner;
std::mutex g_state_mutex;
std::mutex g_hook_mutex;
HookSnapshot g_state;
std::vector<void *> g_hook_targets;
void *g_d3d11_target = nullptr;
void *g_d3d12_target = nullptr;
bool g_attempted = false;
std::atomic<abi::Evaluate11> g_original11{nullptr};
std::atomic<abi::Evaluate11C> g_original11c{nullptr};
std::atomic<abi::Evaluate12> g_original12{nullptr};
std::atomic<abi::Evaluate12C> g_original12c{nullptr};
thread_local bool g_inside_hook = false;

void log(reshade::log::level level, const std::string &message)
{
    reshade::log::message(level,
        (std::string("[FrameCompare DLSS5] ") + message).c_str());
}

void set_error(std::string message)
{
    {
        std::lock_guard lock(g_state_mutex);
        g_state.last_error = message;
    }
    log(reshade::log::level::warning, message);
}

bool compatible_filename(std::wstring path)
{
    std::transform(path.begin(), path.end(), path.begin(),
        [](wchar_t value) { return static_cast<wchar_t>(std::towlower(value)); });
    return path.find(L"nvngx.dll") != std::wstring::npos;
}

template <typename Resource>
bool get_resource(const abi::Parameter *parameters, const char *key,
                  Resource **resource) noexcept
{
    *resource = nullptr;
    if (parameters == nullptr)
        return false;
#if defined(_MSC_VER)
    __try
    {
        return abi::succeeded(parameters->Get(key, resource)) &&
               *resource != nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        *resource = nullptr;
        return false;
    }
#else
    return abi::succeeded(parameters->Get(key, resource)) &&
           *resource != nullptr;
#endif
}

void dimensions(ID3D11Resource *resource, std::uint64_t &width,
                std::uint32_t &height) noexcept
{
    if (resource == nullptr) return;
#if defined(_MSC_VER)
    __try
    {
#endif
        ID3D11Texture2D *texture = nullptr;
        if (SUCCEEDED(resource->QueryInterface(IID_PPV_ARGS(&texture))) &&
            texture != nullptr)
        {
            D3D11_TEXTURE2D_DESC desc{};
            texture->GetDesc(&desc);
            texture->Release();
            width = desc.Width;
            height = desc.Height;
        }
#if defined(_MSC_VER)
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { width = 0; height = 0; }
#endif
}

void dimensions(ID3D12Resource *resource, std::uint64_t &width,
                std::uint32_t &height) noexcept
{
    if (resource == nullptr) return;
#if defined(_MSC_VER)
    __try
    {
#endif
        const D3D12_RESOURCE_DESC desc = resource->GetDesc();
        width = desc.Width;
        height = desc.Height;
#if defined(_MSC_VER)
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { width = 0; height = 0; }
#endif
}

template <typename Resource>
EvaluationInfo observe(const abi::Parameter *parameters)
{
    Resource *color = nullptr;
    Resource *output = nullptr;
    const char *color_key = "DLSSNR.Color";
    const char *output_key = "DLSSNR.Output";
    if (!get_resource(parameters, color_key, &color))
    {
        color_key = "Color";
        get_resource(parameters, color_key, &color);
    }
    if (!get_resource(parameters, output_key, &output))
    {
        output_key = "Output";
        get_resource(parameters, output_key, &output);
    }
    EvaluationInfo info;
    info.color = color;
    info.output = output;
    info.color_key = color_key;
    info.output_key = output_key;
    dimensions(output, info.width, info.height);
    return info;
}

void record_observation(Api api, const char *symbol,
                        const EvaluationInfo &info, abi::Result result)
{
    std::lock_guard lock(g_state_mutex);
    auto &calls = api == Api::d3d11 ? g_state.d3d11_calls
                                    : g_state.d3d12_calls;
    ++calls;
    if (info.color != nullptr && info.output != nullptr)
        ++g_state.resource_pairs;
    else
        ++g_state.incomplete_calls;
    char message[512]{};
    std::snprintf(message, sizeof(message),
        "%s #%llu %s result=0x%08X %s=%p %s=%p %llux%u",
        api == Api::d3d11 ? "D3D11" : "D3D12",
        static_cast<unsigned long long>(calls), symbol, result,
        info.color_key, info.color, info.output_key, info.output,
        static_cast<unsigned long long>(info.width), info.height);
    g_state.last_observation = message;
}

void apply(Api api, void *commands, const EvaluationInfo &info,
           abi::Result result) noexcept
{
    const SettingsSnapshot settings = settings_snapshot();
    if (!settings.enabled)
        return;
    CopyRegion region{};
    CopyOutcome outcome = CopyOutcome::incomplete_parameters;
    if (abi::succeeded(result) && info.color != nullptr &&
        info.output != nullptr &&
        info.width <= std::numeric_limits<std::uint32_t>::max())
    {
        region = make_copy_region(static_cast<std::uint32_t>(info.width),
                                  info.height, settings.split_position,
                                  settings.before_on_left);
        if (region.empty())
            outcome = CopyOutcome::empty_region;
        else if (api == Api::d3d11)
            outcome = apply_d3d11_copy(
                static_cast<ID3D11DeviceContext *>(commands),
                static_cast<ID3D11Resource *>(info.color),
                static_cast<ID3D11Resource *>(info.output), region);
        else
            outcome = apply_d3d12_copy(
                static_cast<ID3D12GraphicsCommandList *>(commands),
                static_cast<ID3D12Resource *>(info.color),
                static_cast<ID3D12Resource *>(info.output), region);
    }
    record_result(api, outcome, region);
}

struct HookGuard
{
    HookGuard() { g_inside_hook = true; }
    ~HookGuard() { g_inside_hook = false; }
};

abi::Result FRAMECOMPARE_NGX_CALL hook11(
    ID3D11DeviceContext *context, const abi::Handle *handle,
    const abi::Parameter *parameters, abi::ProgressCallback callback)
{
    const auto original = g_original11.load();
    if (original == nullptr) return 0xBAD00000u;
    if (g_inside_hook) return original(context, handle, parameters, callback);
    HookGuard guard;
    const EvaluationInfo info = observe<ID3D11Resource>(parameters);
    const abi::Result result = original(context, handle, parameters, callback);
    try { record_observation(Api::d3d11, "EvaluateFeature", info, result); }
    catch (...) {}
    apply(Api::d3d11, context, info, result);
    return result;
}

abi::Result FRAMECOMPARE_NGX_CALL hook11c(
    ID3D11DeviceContext *context, const abi::Handle *handle,
    const abi::Parameter *parameters, abi::ProgressCallbackC callback)
{
    const auto original = g_original11c.load();
    if (original == nullptr) return 0xBAD00000u;
    if (g_inside_hook) return original(context, handle, parameters, callback);
    HookGuard guard;
    const EvaluationInfo info = observe<ID3D11Resource>(parameters);
    const abi::Result result = original(context, handle, parameters, callback);
    try { record_observation(Api::d3d11, "EvaluateFeature_C", info, result); }
    catch (...) {}
    apply(Api::d3d11, context, info, result);
    return result;
}

abi::Result FRAMECOMPARE_NGX_CALL hook12(
    ID3D12GraphicsCommandList *commands, const abi::Handle *handle,
    const abi::Parameter *parameters, abi::ProgressCallback callback)
{
    const auto original = g_original12.load();
    if (original == nullptr) return 0xBAD00000u;
    if (g_inside_hook) return original(commands, handle, parameters, callback);
    HookGuard guard;
    const EvaluationInfo info = observe<ID3D12Resource>(parameters);
    const abi::Result result = original(commands, handle, parameters, callback);
    try { record_observation(Api::d3d12, "EvaluateFeature", info, result); }
    catch (...) {}
    apply(Api::d3d12, commands, info, result);
    return result;
}

abi::Result FRAMECOMPARE_NGX_CALL hook12c(
    ID3D12GraphicsCommandList *commands, const abi::Handle *handle,
    const abi::Parameter *parameters, abi::ProgressCallbackC callback)
{
    const auto original = g_original12c.load();
    if (original == nullptr) return 0xBAD00000u;
    if (g_inside_hook) return original(commands, handle, parameters, callback);
    HookGuard guard;
    const EvaluationInfo info = observe<ID3D12Resource>(parameters);
    const abi::Result result = original(commands, handle, parameters, callback);
    try { record_observation(Api::d3d12, "EvaluateFeature_C", info, result); }
    catch (...) {}
    apply(Api::d3d12, commands, info, result);
    return result;
}

void set_hook_flag(HookFlag flag, bool value)
{
    std::lock_guard lock(g_state_mutex);
    switch (flag)
    {
    case HookFlag::d3d11: g_state.d3d11_evaluate = value; break;
    case HookFlag::d3d11c: g_state.d3d11_evaluate_c = value; break;
    case HookFlag::d3d12: g_state.d3d12_evaluate = value; break;
    case HookFlag::d3d12c: g_state.d3d12_evaluate_c = value; break;
    }
}

bool appears_detoured(void *target) noexcept
{
    if (target == nullptr) return true;
#if defined(_MSC_VER)
    __try
    {
#endif
        const auto *code = static_cast<const unsigned char *>(target);
        return code[0] == 0xE9 || code[0] == 0xEB ||
            (code[0] == 0xFF && code[1] == 0x25) ||
            (code[0] == 0x48 && code[1] == 0xB8 &&
             code[10] == 0xFF && code[11] == 0xE0) ||
            (code[0] == 0x49 && code[1] == 0xBB && code[10] == 0x41 &&
             code[11] == 0xFF && code[12] == 0xE3);
#if defined(_MSC_VER)
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return true; }
#endif
}

template <typename Function>
void install_export(HMODULE module, const char *symbol, void *detour,
                    std::atomic<Function> &original, void *&api_target,
                    HookFlag flag)
{
    void *target = reinterpret_cast<void *>(GetProcAddress(module, symbol));
    if (target == nullptr) return;
    if (api_target == target)
    {
        set_hook_flag(flag, true);
        return;
    }
    if (appears_detoured(target))
    {
        set_error(std::string("existing detour; skipped ") + symbol);
        return;
    }
    void *trampoline = nullptr;
    const MH_STATUS created = MH_CreateHook(target, detour, &trampoline);
    if (created != MH_OK)
    {
        set_error(std::string("MinHook create failed for ") + symbol +
                  ": " + MH_StatusToString(created));
        return;
    }
    original.store(reinterpret_cast<Function>(trampoline));
    const MH_STATUS enabled = MH_EnableHook(target);
    if (enabled != MH_OK)
    {
        original.store(nullptr);
        MH_RemoveHook(target);
        set_error(std::string("MinHook enable failed for ") + symbol +
                  ": " + MH_StatusToString(enabled));
        return;
    }
    api_target = target;
    g_hook_targets.push_back(target);
    set_hook_flag(flag, true);
}

void scan_once()
{
    HMODULE module = GetModuleHandleW(L"nvngx_dlssnr.dll");
    if (module == nullptr) return;
    {
        std::lock_guard lock(g_state_mutex);
        g_state.module_loaded = true;
    }
    std::lock_guard hook_lock(g_hook_mutex);
    if (g_attempted) return;
    g_attempted = true;
    install_export(module, "NVSDK_NGX_D3D11_EvaluateFeature",
        reinterpret_cast<void *>(&hook11), g_original11, g_d3d11_target,
        HookFlag::d3d11);
    install_export(module, "NVSDK_NGX_D3D11_EvaluateFeature_C",
        reinterpret_cast<void *>(&hook11c), g_original11c, g_d3d11_target,
        HookFlag::d3d11c);
    install_export(module, "NVSDK_NGX_D3D12_EvaluateFeature",
        reinterpret_cast<void *>(&hook12), g_original12, g_d3d12_target,
        HookFlag::d3d12);
    install_export(module, "NVSDK_NGX_D3D12_EvaluateFeature_C",
        reinterpret_cast<void *>(&hook12c), g_original12c, g_d3d12_target,
        HookFlag::d3d12c);
    const HookSnapshot snapshot = hook_snapshot();
    if (!snapshot.d3d11_evaluate && !snapshot.d3d11_evaluate_c &&
        !snapshot.d3d12_evaluate && !snapshot.d3d12_evaluate_c)
        set_error("nvngx_dlssnr.dll loaded but no Evaluate export was hooked");
}

void scanner_loop()
{
    while (g_running.load(std::memory_order_acquire))
    {
        scan_once();
        for (int step = 0; step < 20 && g_running.load(); ++step)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
}

bool start_hooks()
{
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&g_running), &self))
    {
        set_error("could not resolve add-on module path");
        return false;
    }
    std::vector<wchar_t> path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        self, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size() ||
        !compatible_filename(std::wstring(path.data(), length)))
    {
        set_error("add-on filename must contain nvngx.dll; hooks disabled");
        return false;
    }
    bool expected = false;
    if (!g_running.compare_exchange_strong(expected, true)) return true;
    const MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
    {
        g_running = false;
        set_error(std::string("MinHook initialization failed: ") +
                  MH_StatusToString(status));
        return false;
    }
    {
        std::lock_guard lock(g_state_mutex);
        g_state.scanner_running = true;
    }
    try { g_scanner = std::thread(scanner_loop); }
    catch (...)
    {
        g_running = false;
        MH_Uninitialize();
        set_error("module scanner thread could not start");
        return false;
    }
    log(reshade::log::level::info,
        "waiting for nvngx_dlssnr.dll; generic comparison remains available");
    return true;
}

void stop_hooks()
{
    if (!g_running.exchange(false)) return;
    if (g_scanner.joinable()) g_scanner.join();
    std::lock_guard hook_lock(g_hook_mutex);
    for (auto iterator = g_hook_targets.rbegin();
         iterator != g_hook_targets.rend(); ++iterator)
    {
        MH_DisableHook(*iterator);
        MH_RemoveHook(*iterator);
    }
    g_hook_targets.clear();
    g_original11 = nullptr;
    g_original11c = nullptr;
    g_original12 = nullptr;
    g_original12c = nullptr;
    MH_Uninitialize();
    std::lock_guard state_lock(g_state_mutex);
    g_state.scanner_running = false;
}

HookSnapshot hook_snapshot()
{
    std::lock_guard lock(g_state_mutex);
    return g_state;
}
}
