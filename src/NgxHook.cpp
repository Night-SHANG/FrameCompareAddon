#include "NgxHook.hpp"

#include <MinHook.h>
#include <Psapi.h>
#include <dxgi1_2.h>
#include "NgxAbi.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cwctype>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace framecompare::ngx
{
    namespace
    {
        constexpr NVSDK_NGX_Feature k_target_feature = FRAMECOMPARE_NGX_TARGET_FEATURE;

        std::atomic_bool g_running { false };
        std::atomic_bool g_capture_enabled { false };
        std::atomic<uint32_t> g_d3d12_color_state { static_cast<uint32_t>(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) };
        std::thread g_scanner_thread;

        std::shared_mutex g_feature_mutex;
        std::unordered_map<const NVSDK_NGX_Handle *, NVSDK_NGX_Feature> g_features;

        std::mutex g_capture_mutex;
        std::shared_ptr<native_capture> g_slot11;
        std::shared_ptr<native_capture> g_slot12;
        bool g_slot12_ready = false;
        std::shared_ptr<const snapshot> g_latest;
        uint64_t g_generation = 0;

        std::mutex g_diag_mutex;
        diagnostics g_diag;

        std::mutex g_hook_mutex;
        std::vector<void *> g_hook_targets;
        std::wstring g_selected_hook_module;

        using PFN_Create11 = NVSDK_NGX_Result (FRAMECOMPARE_NGX_CALL *)(ID3D11DeviceContext *, NVSDK_NGX_Feature, NVSDK_NGX_Parameter *, NVSDK_NGX_Handle **);
        using PFN_Create12 = NVSDK_NGX_Result (FRAMECOMPARE_NGX_CALL *)(ID3D12GraphicsCommandList *, NVSDK_NGX_Feature, NVSDK_NGX_Parameter *, NVSDK_NGX_Handle **);
        using PFN_Release = NVSDK_NGX_Result (FRAMECOMPARE_NGX_CALL *)(NVSDK_NGX_Handle *);
        using PFN_Eval11 = NVSDK_NGX_Result (FRAMECOMPARE_NGX_CALL *)(ID3D11DeviceContext *, const NVSDK_NGX_Handle *, const NVSDK_NGX_Parameter *, PFN_NVSDK_NGX_ProgressCallback);
        using PFN_Eval12 = NVSDK_NGX_Result (FRAMECOMPARE_NGX_CALL *)(ID3D12GraphicsCommandList *, const NVSDK_NGX_Handle *, const NVSDK_NGX_Parameter *, PFN_NVSDK_NGX_ProgressCallback);
        using PFN_Eval11C = NVSDK_NGX_Result (FRAMECOMPARE_NGX_CALL *)(ID3D11DeviceContext *, const NVSDK_NGX_Handle *, const NVSDK_NGX_Parameter *, PFN_NVSDK_NGX_ProgressCallback_C);
        using PFN_Eval12C = NVSDK_NGX_Result (FRAMECOMPARE_NGX_CALL *)(ID3D12GraphicsCommandList *, const NVSDK_NGX_Handle *, const NVSDK_NGX_Parameter *, PFN_NVSDK_NGX_ProgressCallback_C);

        PFN_Create11 g_orig_create11 = nullptr;
        PFN_Create12 g_orig_create12 = nullptr;
        PFN_Release g_orig_release11 = nullptr;
        PFN_Release g_orig_release12 = nullptr;
        PFN_Eval11 g_orig_eval11 = nullptr;
        PFN_Eval12 g_orig_eval12 = nullptr;
        PFN_Eval11C g_orig_eval11c = nullptr;
        PFN_Eval12C g_orig_eval12c = nullptr;

        void *g_target_eval11 = nullptr;
        void *g_target_eval12 = nullptr;

        std::wstring lower_copy(std::wstring value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
            return value;
        }

        int module_priority(const std::wstring &name)
        {
            const std::wstring n = lower_copy(name);
            if (n == L"_nvngx.dll") return 0;
            if (n == L"nvngx.dll") return 1;
            if (n.find(L"dlssnr") != std::wstring::npos) return 2;
            if (n.find(L"dlss") != std::wstring::npos) return 3;
            if (n.find(L"ngx") != std::wstring::npos) return 4;
            return 10;
        }

        struct module_entry
        {
            HMODULE module = nullptr;
            std::wstring name;
            int priority = 10;
        };

        std::vector<module_entry> enumerate_modules()
        {
            std::vector<module_entry> out;
            DWORD needed = 0;
            std::array<HMODULE, 1024> modules {};
            if (!K32EnumProcessModules(GetCurrentProcess(), modules.data(), static_cast<DWORD>(sizeof(modules)), &needed))
                return out;

            const size_t count = std::min<size_t>(modules.size(), needed / sizeof(HMODULE));
            out.reserve(count);
            for (size_t i = 0; i < count; ++i)
            {
                wchar_t name[MAX_PATH] = {};
                if (K32GetModuleBaseNameW(GetCurrentProcess(), modules[i], name, MAX_PATH) == 0)
                    continue;
                module_entry e;
                e.module = modules[i];
                e.name = name;
                e.priority = module_priority(e.name);
                out.push_back(std::move(e));
            }
            std::stable_sort(out.begin(), out.end(), [](const module_entry &a, const module_entry &b) {
                return a.priority < b.priority;
            });
            return out;
        }

        void set_error(const std::string &message)
        {
            std::lock_guard lock(g_diag_mutex);
            g_diag.last_error = message;
        }

        void publish_snapshot(const std::shared_ptr<native_capture> &native, uint32_t width, uint32_t height)
        {
            // This function is only called while g_capture_mutex is held. Do not read
            // g_selected_hook_module here: the scanner owns that field under g_hook_mutex.
            // g_diag.hook_module is the synchronized public copy.
            std::wstring hook_module;
            {
                std::lock_guard diag_lock(g_diag_mutex);
                hook_module = g_diag.hook_module;
            }

            auto item = std::make_shared<snapshot>();
            item->native = native;
            item->generation = ++g_generation;
            item->captured_at = std::chrono::steady_clock::now();
            item->hook_module = std::move(hook_module);
            g_latest = item;

            std::lock_guard diag_lock(g_diag_mutex);
            ++g_diag.successful_captures;
            g_diag.latest_generation = item->generation;
            g_diag.latest_api = native ? native->api : snapshot_api::none;
            g_diag.latest_width = width;
            g_diag.latest_height = height;
            g_diag.last_error.clear();
        }

        bool same_desc11(const D3D11_TEXTURE2D_DESC &a, const D3D11_TEXTURE2D_DESC &b)
        {
            return a.Width == b.Width && a.Height == b.Height && a.MipLevels == b.MipLevels &&
                a.ArraySize == b.ArraySize && a.Format == b.Format &&
                a.SampleDesc.Count == b.SampleDesc.Count && a.SampleDesc.Quality == b.SampleDesc.Quality;
        }

        bool same_desc12(const D3D12_RESOURCE_DESC &a, const D3D12_RESOURCE_DESC &b)
        {
            return a.Dimension == b.Dimension && a.Alignment == b.Alignment && a.Width == b.Width &&
                a.Height == b.Height && a.DepthOrArraySize == b.DepthOrArraySize &&
                a.MipLevels == b.MipLevels && a.Format == b.Format &&
                a.SampleDesc.Count == b.SampleDesc.Count && a.SampleDesc.Quality == b.SampleDesc.Quality &&
                a.Layout == b.Layout && a.Flags == b.Flags;
        }

        std::shared_ptr<native_capture> create_snapshot11(ID3D11Device *device, const D3D11_TEXTURE2D_DESC &src_desc)
        {
            D3D11_TEXTURE2D_DESC desc = src_desc;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = 0;
            desc.MiscFlags = 0;

            ComPtr<ID3D11Texture2D> texture;
            if (FAILED(device->CreateTexture2D(&desc, nullptr, &texture)))
                return {};

            auto result = std::make_shared<native_capture>();
            result->api = snapshot_api::d3d11;
            result->device11 = device;
            result->texture11 = texture;
            result->desc11 = desc;
            result->cross_device_shareable = false;
            return result;
        }

        std::shared_ptr<native_capture> create_snapshot12(ID3D12Device *device, const D3D12_RESOURCE_DESC &src_desc)
        {
            D3D12_HEAP_PROPERTIES heap = {};
            heap.Type = D3D12_HEAP_TYPE_DEFAULT;
            heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            heap.CreationNodeMask = 1;
            heap.VisibleNodeMask = 1;

            // Keep the source resource description intact. D3D12 CopyResource is most robust
            // when the source and snapshot descriptions match; sharing is controlled by the
            // heap flag and does not require rewriting the resource flags.
            D3D12_RESOURCE_DESC desc = src_desc;

            ComPtr<ID3D12Resource> texture;
            bool shared = false;
            HRESULT hr = device->CreateCommittedResource(
                &heap, D3D12_HEAP_FLAG_SHARED, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr, IID_PPV_ARGS(&texture));
            if (SUCCEEDED(hr))
            {
                shared = true;
            }
            else
            {
                hr = device->CreateCommittedResource(
                    &heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
                    nullptr, IID_PPV_ARGS(&texture));
                if (FAILED(hr))
                    return {};
            }

            auto result = std::make_shared<native_capture>();
            result->api = snapshot_api::d3d12;
            result->device12 = device;
            result->texture12 = texture;
            result->desc12 = desc;
            result->cross_device_shareable = false;

            if (shared)
            {
                HANDLE handle = nullptr;
                if (SUCCEEDED(device->CreateSharedHandle(texture.Get(), nullptr, GENERIC_ALL, nullptr, &handle)))
                {
                    result->shared_handle = handle;
                    result->cross_device_shareable = true;
                }
            }
            return result;
        }

        bool capture_d3d11(ID3D11DeviceContext *context, const NVSDK_NGX_Parameter *params)
        {
            if (!context || !params)
                return false;

            ID3D11Resource *color = nullptr;
            if (framecompare_ngx_failed(params->Get(FRAMECOMPARE_NGX_PARAMETER_COLOR, &color)) || color == nullptr)
                return false;

            ComPtr<ID3D11Texture2D> source;
            if (FAILED(color->QueryInterface(IID_PPV_ARGS(&source))))
                return false;

            D3D11_TEXTURE2D_DESC src_desc = {};
            source->GetDesc(&src_desc);
            if (src_desc.SampleDesc.Count != 1)
                return false;

            ComPtr<ID3D11Device> device;
            context->GetDevice(&device);
            if (!device)
                return false;

            std::lock_guard lock(g_capture_mutex);
            if (!g_slot11 || g_slot11->device11.Get() != device.Get() || !same_desc11(g_slot11->desc11, src_desc))
            {
                g_slot11 = create_snapshot11(device.Get(), src_desc);
                if (!g_slot11)
                    return false;
            }

            context->CopyResource(g_slot11->texture11.Get(), source.Get());
            publish_snapshot(g_slot11, src_desc.Width, src_desc.Height);
            return true;
        }

        D3D12_RESOURCE_BARRIER transition_barrier(ID3D12Resource *resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
        {
            D3D12_RESOURCE_BARRIER b = {};
            b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            b.Transition.pResource = resource;
            b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            b.Transition.StateBefore = before;
            b.Transition.StateAfter = after;
            return b;
        }

        bool capture_d3d12(ID3D12GraphicsCommandList *cmd, const NVSDK_NGX_Parameter *params)
        {
            if (!cmd || !params)
                return false;

            ID3D12Resource *color = nullptr;
            if (framecompare_ngx_failed(params->Get(FRAMECOMPARE_NGX_PARAMETER_COLOR, &color)) || color == nullptr)
                return false;

            const D3D12_RESOURCE_DESC src_desc = color->GetDesc();
            if (src_desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || src_desc.SampleDesc.Count != 1)
                return false;

            ComPtr<ID3D12Device> device;
            if (FAILED(cmd->GetDevice(IID_PPV_ARGS(&device))) || !device)
                return false;

            std::lock_guard lock(g_capture_mutex);
            if (!g_slot12 || g_slot12->device12.Get() != device.Get() || !same_desc12(g_slot12->desc12, src_desc))
            {
                g_slot12 = create_snapshot12(device.Get(), src_desc);
                g_slot12_ready = false;
                if (!g_slot12)
                    return false;
            }

            const auto configured_state = static_cast<D3D12_RESOURCE_STATES>(g_d3d12_color_state.load(std::memory_order_relaxed));
            std::array<D3D12_RESOURCE_BARRIER, 2> before {};
            UINT before_count = 0;
            if (configured_state != D3D12_RESOURCE_STATE_COPY_SOURCE)
                before[before_count++] = transition_barrier(color, configured_state, D3D12_RESOURCE_STATE_COPY_SOURCE);
            if (g_slot12_ready)
                before[before_count++] = transition_barrier(g_slot12->texture12.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
            if (before_count != 0)
                cmd->ResourceBarrier(before_count, before.data());

            cmd->CopyResource(g_slot12->texture12.Get(), color);

            std::array<D3D12_RESOURCE_BARRIER, 2> after {};
            UINT after_count = 0;
            after[after_count++] = transition_barrier(g_slot12->texture12.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
            if (configured_state != D3D12_RESOURCE_STATE_COPY_SOURCE)
                after[after_count++] = transition_barrier(color, D3D12_RESOURCE_STATE_COPY_SOURCE, configured_state);
            cmd->ResourceBarrier(after_count, after.data());
            g_slot12_ready = true;

            publish_snapshot(g_slot12, static_cast<uint32_t>(src_desc.Width), src_desc.Height);
            return true;
        }

        bool is_feature18(const NVSDK_NGX_Handle *handle)
        {
            std::shared_lock lock(g_feature_mutex);
            const auto it = g_features.find(handle);
            return it != g_features.end() && it->second == k_target_feature;
        }

        void record_feature(const NVSDK_NGX_Handle *handle, NVSDK_NGX_Feature feature)
        {
            if (!handle)
                return;
            {
                std::unique_lock lock(g_feature_mutex);
                g_features[handle] = feature;
            }
            if (feature == k_target_feature)
            {
                std::lock_guard diag_lock(g_diag_mutex);
                ++g_diag.feature18_creates;
            }
        }

        void remove_feature(const NVSDK_NGX_Handle *handle)
        {
            std::unique_lock lock(g_feature_mutex);
            g_features.erase(handle);
        }

        void before_evaluate11(ID3D11DeviceContext *ctx, const NVSDK_NGX_Handle *handle, const NVSDK_NGX_Parameter *params)
        {
            if (!g_capture_enabled.load(std::memory_order_relaxed))
                return;
            if (!is_feature18(handle))
            {
                std::shared_lock lock(g_feature_mutex);
                if (g_features.find(handle) == g_features.end())
                {
                    std::lock_guard diag_lock(g_diag_mutex);
                    ++g_diag.unknown_handle_evaluates;
                }
                return;
            }
            {
                std::lock_guard diag_lock(g_diag_mutex);
                ++g_diag.feature18_evaluates;
            }
            if (!capture_d3d11(ctx, params))
            {
                std::lock_guard diag_lock(g_diag_mutex);
                ++g_diag.failed_captures;
                g_diag.last_error = "Feature 18 D3D11 Color capture failed";
            }
        }

        void before_evaluate12(ID3D12GraphicsCommandList *cmd, const NVSDK_NGX_Handle *handle, const NVSDK_NGX_Parameter *params)
        {
            if (!g_capture_enabled.load(std::memory_order_relaxed))
                return;
            if (!is_feature18(handle))
            {
                std::shared_lock lock(g_feature_mutex);
                if (g_features.find(handle) == g_features.end())
                {
                    std::lock_guard diag_lock(g_diag_mutex);
                    ++g_diag.unknown_handle_evaluates;
                }
                return;
            }
            {
                std::lock_guard diag_lock(g_diag_mutex);
                ++g_diag.feature18_evaluates;
            }
            if (!capture_d3d12(cmd, params))
            {
                std::lock_guard diag_lock(g_diag_mutex);
                ++g_diag.failed_captures;
                g_diag.last_error = "Feature 18 D3D12 Color capture failed";
            }
        }

        NVSDK_NGX_Result FRAMECOMPARE_NGX_CALL hook_create11(ID3D11DeviceContext *ctx, NVSDK_NGX_Feature feature, NVSDK_NGX_Parameter *params, NVSDK_NGX_Handle **out_handle)
        {
            const NVSDK_NGX_Result result = g_orig_create11(ctx, feature, params, out_handle);
            if (framecompare_ngx_succeeded(result) && out_handle && *out_handle)
                record_feature(*out_handle, feature);
            return result;
        }

        NVSDK_NGX_Result FRAMECOMPARE_NGX_CALL hook_create12(ID3D12GraphicsCommandList *cmd, NVSDK_NGX_Feature feature, NVSDK_NGX_Parameter *params, NVSDK_NGX_Handle **out_handle)
        {
            const NVSDK_NGX_Result result = g_orig_create12(cmd, feature, params, out_handle);
            if (framecompare_ngx_succeeded(result) && out_handle && *out_handle)
                record_feature(*out_handle, feature);
            return result;
        }

        NVSDK_NGX_Result FRAMECOMPARE_NGX_CALL hook_release11(NVSDK_NGX_Handle *handle)
        {
            remove_feature(handle);
            return g_orig_release11(handle);
        }

        NVSDK_NGX_Result FRAMECOMPARE_NGX_CALL hook_release12(NVSDK_NGX_Handle *handle)
        {
            remove_feature(handle);
            return g_orig_release12(handle);
        }

        NVSDK_NGX_Result FRAMECOMPARE_NGX_CALL hook_eval11(ID3D11DeviceContext *ctx, const NVSDK_NGX_Handle *handle, const NVSDK_NGX_Parameter *params, PFN_NVSDK_NGX_ProgressCallback callback)
        {
            before_evaluate11(ctx, handle, params);
            return g_orig_eval11(ctx, handle, params, callback);
        }

        NVSDK_NGX_Result FRAMECOMPARE_NGX_CALL hook_eval12(ID3D12GraphicsCommandList *cmd, const NVSDK_NGX_Handle *handle, const NVSDK_NGX_Parameter *params, PFN_NVSDK_NGX_ProgressCallback callback)
        {
            before_evaluate12(cmd, handle, params);
            return g_orig_eval12(cmd, handle, params, callback);
        }

        NVSDK_NGX_Result FRAMECOMPARE_NGX_CALL hook_eval11c(ID3D11DeviceContext *ctx, const NVSDK_NGX_Handle *handle, const NVSDK_NGX_Parameter *params, PFN_NVSDK_NGX_ProgressCallback_C callback)
        {
            before_evaluate11(ctx, handle, params);
            return g_orig_eval11c(ctx, handle, params, callback);
        }

        NVSDK_NGX_Result FRAMECOMPARE_NGX_CALL hook_eval12c(ID3D12GraphicsCommandList *cmd, const NVSDK_NGX_Handle *handle, const NVSDK_NGX_Parameter *params, PFN_NVSDK_NGX_ProgressCallback_C callback)
        {
            before_evaluate12(cmd, handle, params);
            return g_orig_eval12c(cmd, handle, params, callback);
        }

        template <typename T>
        bool install_hook_for_export(const std::vector<module_entry> &modules, const char *symbol, void *detour, T &original, bool &diag_flag, void **out_target = nullptr, void *duplicate_target = nullptr)
        {
            if (original != nullptr || diag_flag)
                return true;

            for (const module_entry &entry : modules)
            {
                FARPROC proc = GetProcAddress(entry.module, symbol);
                if (!proc)
                    continue;

                void *target = reinterpret_cast<void *>(proc);
                if (duplicate_target != nullptr && target == duplicate_target)
                {
                    diag_flag = true; // The sibling export aliases the already hooked function.
                    if (out_target) *out_target = target;
                    return true;
                }

                void *trampoline = nullptr;
                const MH_STATUS create_status = MH_CreateHook(target, detour, &trampoline);
                if (create_status != MH_OK)
                {
                    set_error(std::string("MinHook create failed for ") + symbol + ": " + MH_StatusToString(create_status));
                    continue;
                }
                // Publish the trampoline before enabling the detour. Another render thread may
                // enter the hook immediately after MH_EnableHook returns.
                original = reinterpret_cast<T>(trampoline);
                const MH_STATUS enable_status = MH_EnableHook(target);
                if (enable_status != MH_OK)
                {
                    original = nullptr;
                    MH_RemoveHook(target);
                    set_error(std::string("MinHook enable failed for ") + symbol + ": " + MH_StatusToString(enable_status));
                    continue;
                }

                if (out_target) *out_target = target;
                g_hook_targets.push_back(target);
                if (g_selected_hook_module.empty() || entry.priority < module_priority(g_selected_hook_module))
                    g_selected_hook_module = entry.name;
                diag_flag = true;
                return true;
            }
            return false;
        }

        void scan_and_hook()
        {
            const auto modules = enumerate_modules();
            if (modules.empty())
                return;

            std::lock_guard hook_lock(g_hook_mutex);
            bool c11, c12, e11, e11c, e12, e12c, r11, r12;
            {
                std::lock_guard diag_lock(g_diag_mutex);
                c11 = g_diag.create11_hooked; c12 = g_diag.create12_hooked;
                e11 = g_diag.eval11_hooked; e11c = g_diag.eval11_c_hooked;
                e12 = g_diag.eval12_hooked; e12c = g_diag.eval12_c_hooked;
                r11 = g_diag.release11_hooked; r12 = g_diag.release12_hooked;
            }

            install_hook_for_export(modules, "NVSDK_NGX_D3D11_CreateFeature", reinterpret_cast<void *>(&hook_create11), g_orig_create11, c11);
            install_hook_for_export(modules, "NVSDK_NGX_D3D12_CreateFeature", reinterpret_cast<void *>(&hook_create12), g_orig_create12, c12);
            install_hook_for_export(modules, "NVSDK_NGX_D3D11_EvaluateFeature", reinterpret_cast<void *>(&hook_eval11), g_orig_eval11, e11, &g_target_eval11);
            install_hook_for_export(modules, "NVSDK_NGX_D3D12_EvaluateFeature", reinterpret_cast<void *>(&hook_eval12), g_orig_eval12, e12, &g_target_eval12);
            install_hook_for_export(modules, "NVSDK_NGX_D3D11_EvaluateFeature_C", reinterpret_cast<void *>(&hook_eval11c), g_orig_eval11c, e11c, nullptr, g_target_eval11);
            install_hook_for_export(modules, "NVSDK_NGX_D3D12_EvaluateFeature_C", reinterpret_cast<void *>(&hook_eval12c), g_orig_eval12c, e12c, nullptr, g_target_eval12);
            install_hook_for_export(modules, "NVSDK_NGX_D3D11_ReleaseFeature", reinterpret_cast<void *>(&hook_release11), g_orig_release11, r11);
            install_hook_for_export(modules, "NVSDK_NGX_D3D12_ReleaseFeature", reinterpret_cast<void *>(&hook_release12), g_orig_release12, r12);

            std::lock_guard diag_lock(g_diag_mutex);
            g_diag.create11_hooked = c11; g_diag.create12_hooked = c12;
            g_diag.eval11_hooked = e11; g_diag.eval11_c_hooked = e11c;
            g_diag.eval12_hooked = e12; g_diag.eval12_c_hooked = e12c;
            g_diag.release11_hooked = r11; g_diag.release12_hooked = r12;
            if (!g_selected_hook_module.empty())
                g_diag.hook_module = g_selected_hook_module;
        }

        void scanner_loop()
        {
            while (g_running.load(std::memory_order_acquire))
            {
                scan_and_hook();
                for (int i = 0; i < 10 && g_running.load(std::memory_order_acquire); ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
        }
    }

    native_capture::~native_capture()
    {
        if (shared_handle)
        {
            CloseHandle(shared_handle);
            shared_handle = nullptr;
        }
    }

    bool initialize()
    {
        if (g_running.exchange(true, std::memory_order_acq_rel))
            return true;

        const MH_STATUS status = MH_Initialize();
        if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
        {
            g_running.store(false, std::memory_order_release);
            set_error(std::string("MinHook initialization failed: ") + MH_StatusToString(status));
            return false;
        }

        {
            std::lock_guard diag_lock(g_diag_mutex);
            g_diag.initialized = true;
        }
        scan_and_hook();
        g_scanner_thread = std::thread(scanner_loop);
        return true;
    }

    void shutdown()
    {
        // Stop doing GPU work before detours are removed.
        g_capture_enabled.store(false, std::memory_order_release);
        if (!g_running.exchange(false, std::memory_order_acq_rel))
            return;
        if (g_scanner_thread.joinable())
            g_scanner_thread.join();

        {
            std::lock_guard hook_lock(g_hook_mutex);
            for (void *target : g_hook_targets)
            {
                MH_DisableHook(target);
                MH_RemoveHook(target);
            }
            g_hook_targets.clear();
        }
        MH_Uninitialize();

        {
            std::unique_lock feature_lock(g_feature_mutex);
            g_features.clear();
        }
        {
            std::lock_guard capture_lock(g_capture_mutex);
            g_latest.reset();
            g_slot11.reset();
            g_slot12.reset();
            g_slot12_ready = false;
        }
        {
            std::lock_guard diag_lock(g_diag_mutex);
            g_diag.initialized = false;
            g_diag.capture_enabled = false;
        }
    }

    void set_capture_enabled(bool enabled)
    {
        g_capture_enabled.store(enabled, std::memory_order_release);
        std::lock_guard diag_lock(g_diag_mutex);
        g_diag.capture_enabled = enabled;
    }

    void set_d3d12_color_state(D3D12_RESOURCE_STATES state)
    {
        g_d3d12_color_state.store(static_cast<uint32_t>(state), std::memory_order_release);
    }

    std::shared_ptr<const snapshot> latest_snapshot()
    {
        std::lock_guard capture_lock(g_capture_mutex);
        return g_latest;
    }

    diagnostics get_diagnostics()
    {
        std::lock_guard diag_lock(g_diag_mutex);
        return g_diag;
    }
}
