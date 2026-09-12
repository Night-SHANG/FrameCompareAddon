# DLSS5 Before Source Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an optional DLSS5-aware Before source to FrameCompare v2 while preserving every existing generic comparison behavior when the option is disabled or unavailable.

**Architecture:** The NGX integration observes D3D11/D3D12 DLSSNR Evaluate calls and, after a successful call, copies only the current Before-side region from `DLSSNR.Color` into `DLSSNR.Output`. FrameCompare's existing Pre-ReShade capture and compositor remain authoritative, so all existing split direction, labels, sweep, hotkeys, persistence, and Post-ReShade After behavior are reused. A lock-free settings snapshot connects the UI-owned `CompositorSettings` to the hook thread; failures always skip the copy and leave the original NGX result unchanged.

**Tech Stack:** C++17, ReShade add-on API 6.8.0, MinHook 1.3.4, D3D11, D3D12, CMake, GitHub Actions.

**Spec:** `../RenoDxNgxObserver/docs/superpowers/specs/2026-09-13-dlss5-framecompare-integration-design.md` plus the later product decision that this is one persisted checkbox rather than a separate source selector.

## Global Constraints

- The DLSS5 option defaults to disabled; disabled behavior must match FrameCompare v2.0.0.
- The existing generic Pre-ReShade Before capture stays in place.
- The DLSS5 copy is effective only when comparison is enabled and display mode is same-coordinate.
- Center-remap mode falls back to the generic Before source and displays a localized warning.
- D3D11/D3D12 resource checks require matching 2D, non-MSAA textures on the same device.
- Hook, module, parameter, state, or copy failures are fail-open and never change the NGX Evaluate result.
- The add-on does not perform CPU readback, create a command queue, dispatch DLSS, or modify NGX parameters.
- The output filename contains `nvngx.dll` so RenoDX caller-module compatibility checks accept it.
- Both `.addon64` and `.addon32` continue to build; runtime DLSS5 support is only claimed where validated.
- ReShade and MinHook licenses and versions are documented in `THIRD_PARTY.md`.

---

### Task 1: Pure DLSS5 Region Model and Runtime Snapshot

**Files:**
- Create: `src/integrations/dlss5/model.hpp`
- Create: `src/integrations/dlss5/model.cpp`
- Create: `src/integrations/dlss5/runtime.hpp`
- Create: `src/integrations/dlss5/runtime.cpp`
- Create: `tests/dlss5_model_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `validate_copy`, `make_copy_region`, `publish_settings`, `settings_snapshot`, `record_result`, and `diagnostics_snapshot`.
- Consumes: plain dimensions, formats, sample counts, and the authoritative FrameCompare split settings.

- [ ] Write tests for left/right split region calculation, empty edge regions, incompatible texture rejection, and settings/diagnostics snapshots.
- [ ] Compile the new test before implementation and confirm it fails because the DLSS5 interfaces do not exist.
- [ ] Implement the pure model and atomic runtime snapshots without Direct3D or ReShade dependencies.
- [ ] Compile and run the new test, then run all existing core tests.

### Task 2: D3D11 and D3D12 In-Place Copy Backends

**Files:**
- Create: `src/integrations/dlss5/gpu/d3d11_copy.hpp`
- Create: `src/integrations/dlss5/gpu/d3d11_copy.cpp`
- Create: `src/integrations/dlss5/gpu/d3d12_copy.hpp`
- Create: `src/integrations/dlss5/gpu/d3d12_copy.cpp`

**Interfaces:**
- Consumes: native `DLSSNR.Color`, `DLSSNR.Output`, a supplied D3D11 context or D3D12 command list, and the pure model's `CopyRegion`.
- Produces: a `CopyOutcome` that records applied, disabled, incomplete, incompatible, empty-region, or guarded-failure results.

- [ ] Adapt the already runtime-validated observer copy logic under the FrameCompare namespace.
- [ ] Preserve D3D12 source/output states around `CopyTextureRegion` and insert the required output UAV barrier.
- [ ] Guard native calls so invalid third-party resources skip comparison without crashing the game.
- [ ] Add static audit checks for the required transitions, copy calls, and absence of CPU readback/dispatch code.

### Task 3: NGX Export Observation and Hook Lifecycle

**Files:**
- Create: `src/integrations/dlss5/ngx_abi.hpp`
- Create: `src/integrations/dlss5/hook_manager.hpp`
- Create: `src/integrations/dlss5/hook_manager.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `nvngx_dlssnr.dll` Evaluate exports, NGX parameter names, runtime settings snapshot, and GPU copy backends.
- Produces: `start_hooks`, `stop_hooks`, `hook_snapshot`, and diagnostic counters for the UI.

- [ ] Add the minimal public NGX ABI definitions used by the validated observer.
- [ ] Hook D3D11/D3D12 Evaluate and Evaluate_C exports through MinHook after the original call succeeds.
- [ ] Read `DLSSNR.Color` and `DLSSNR.Output`, with `Color`/`Output` fallback, then invoke the correct backend.
- [ ] Keep a background module watcher, reject incompatible caller filenames, avoid double hooks, and cleanly remove owned hooks during unload.
- [ ] Fetch and link MinHook 1.3.4 plus D3D11/D3D12 in the add-on target.

### Task 4: Authoritative Setting, Persistence, and Lifecycle Integration

**Files:**
- Modify: `src/render/compositor_params.hpp`
- Modify: `src/config/settings_encode.cpp`
- Modify: `src/config/settings_decode.cpp`
- Modify: `src/addon_entry.cpp`
- Modify: `FrameCompare.ini.example`

**Interfaces:**
- Consumes: `CompositorSettings::dlss5_before` as the only editable source of truth.
- Produces: persisted `[DLSS5] Enabled`, derived hook settings, and fail-open hook startup/shutdown.

- [ ] Add `dlss5_before = false` to `CompositorSettings` and extend codec tests before production changes.
- [ ] Save and load `[DLSS5] Enabled` with a false default.
- [ ] Publish an effective snapshot only when comparison, the checkbox, and same-coordinate mode are all enabled.
- [ ] Start hook observation after configuration initialization without making add-on registration depend on hook success.
- [ ] Stop hooks before ReShade add-on shutdown and preserve all generic capture events unchanged.

### Task 5: Localized Compare UI and Diagnostics

**Files:**
- Modify: `src/ui/i18n/localization.hpp`
- Modify: `src/ui/i18n/localization.cpp`
- Modify: `src/ui/pages/panel_pages.cpp`
- Modify: `tests/localization_tests.cpp`

**Interfaces:**
- Consumes: the authoritative checkbox, derived compatibility state, `HookSnapshot`, and `DiagnosticsSnapshot`.
- Produces: Chinese/English checkbox help, center-mode fallback warning, hook status, copy counters, last result, and last error.

- [ ] Add localization tests for every new stable text key and both supported languages.
- [ ] Add the DLSS5 checkbox to the comparison page and save changes through the existing configuration flow.
- [ ] Display a warning when center-remap prevents DLSS5 copy while leaving the user's saved checkbox unchanged.
- [ ] Display compact read-only module/hook/copy diagnostics sufficient for in-game validation.

### Task 6: Packaging, Documentation, and Static Audit

**Files:**
- Create: `THIRD_PARTY.md`
- Create: `tools/audits/dlss5_audit.py`
- Modify: `tools/static_audit.py`
- Modify: `README.md`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: final filenames, dependency versions, runtime limitations, and installation conflict rules.
- Produces: package-ready legal notices, install/test instructions, and CI-verifiable integration invariants.

- [ ] Rename outputs to `00-nvngx.dll-FrameCompare-v2.addon64` and `.addon32` and include `THIRD_PARTY.md` in packages.
- [ ] Document removal of the old FrameCompare binary and standalone RenoDxNgxObserver before testing.
- [ ] Document unchecked/checked behavior, center-mode fallback, diagnostics, fail-open behavior, and D3D11 validation status.
- [ ] Add a focused static audit module under 200 lines and call it from the existing audit entrypoint.

### Task 7: Full Verification and Main-Branch Delivery

**Files:**
- Verify all changed files.

**Interfaces:**
- Consumes: the complete implementation.
- Produces: reproducible local evidence and GitHub Actions x86/x64 artifacts from `main`.

- [ ] Compile and run all eight core tests with the available local C++ compiler.
- [ ] Run `tools/static_audit.py`, source-folder file-count checks, changed Python line-count checks, UTF-8 checks, and `git diff --check`.
- [ ] Review the complete diff for disabled-mode compatibility and fail-open behavior.
- [ ] Commit the integrated feature to `main` and push it to the private GitHub repository.
- [ ] Inspect the resulting GitHub Actions run; if compilation fails, use its logs to fix the implementation and repeat verification.

