# FrameCompare v1.1 static audit

Audit date: 2026-09-08

This source package was statically reviewed in a Linux container. The container does not provide the Windows SDK/MSVC/ReShade runtime, so this document deliberately distinguishes source/API review from an actual Windows build or in-game test.

## Completed review

- ReShade API guard added: API 20 or newer is required.
- Add-on initialization uses `AddonInit`/`AddonUninit`; heavy NGX/MinHook initialization is not performed from a custom `DllMain`.
- ReShade callbacks use init/destroy runtime, generic present, begin/finish effects, ReShade present, effects reloaded and overlay-open notifications.
- The compositor technique is explicitly disabled in the normal preset list and rendered manually after the After capture.
- Runtime parameters use an 8x1 GPU texture updated through `command_list::update_texture_region`, avoiding dependence on mutable ReShade uniforms/Performance Mode.
- Label placement preview reuses the same GPU label textures/compositor as final recording. When no valid pair exists it copies the current post-effects render target into the After capture as a neutral preview base; it does not depend on an ImGui text overlay for the recorded label path.
- Before/After resources are GPU copies and are transitioned between copy and shader-resource states.
- D3D11 NGX capture copies the `Color` resource before original EvaluateFeature.
- D3D12 NGX capture records source transition → snapshot copy → source restore before original EvaluateFeature on the same command list.
- D3D12 snapshot description now preserves the source resource description for CopyResource compatibility.
- Cross-device NGX sharing is disabled by default because shared handles do not provide queue synchronization.
- Feature handles are only classified from observed CreateFeature calls. Unknown handles are not guessed to be Feature 18.
- NGX scanner/capture shared state uses mutexes/atomics; hook trampoline pointers are published before the detours are enabled.
- No NVIDIA SDK header/source is included in the repository.
- No user-provided SplitScreenCR shader or `sMask.png` is redistributed.
- No `TODO`, `FIXME`, old `v0.1` names or old `zz-FrameCompare` output names remain in the final source tree.

## API facts verified against current public sources

- Current ReShade `reshade.hpp` reports `RESHADE_API_VERSION 20`.
- Current ReShade `effect_runtime` exposes `find_technique`, `set_technique_state`, `render_technique`, input methods, `block_input_next_frame`, and texture semantic binding support used by this project.
- Current ReShade command-list API supports copy/barrier/texture-region update operations used here.
- NVIDIA's current public NGX parameter declaration has 8 Set overloads, 8 Get overloads, then Reset, matching the minimal ABI declaration in `NgxAbi.hpp`.
- NVIDIA's current public feature enum names numeric 18 `NVSDK_NGX_Feature_Reserved18`; FrameCompare intentionally treats numeric 18 as the target because that is the private/current DLSS5 ecosystem convention being tested, not because NVIDIA's public header names it Neural Rendering.

## Known technical boundaries, not source bugs

1. Numeric Feature 18 is officially reserved in NVIDIA's public header. Its DLSS5 meaning is an ecosystem/private-interface assumption and can change.
2. A Feature-18 `Color` input can be an internal resolution/intermediate. It is pre-Evaluate, but not universally equivalent to uninstalling all enhancement components and re-rendering the game.
3. D3D12 resource state cannot be universally queried from an arbitrary application resource, so `NGX.D3D12ColorState` is configurable.
4. Add-on callback/hook ordering between independently injected components cannot be universally forced. `00-` naming is only a best-effort load-order aid.
5. Native Vulkan NGX Feature-18 interception is not implemented in v1.1; generic ReShade capture modes remain available.
6. ReShade's own screenshot capture may occur before this late composite; external recording is preferred.

## Validation still requiring Windows

- MSVC compile/link against the exact ReShade checkout fetched by CMake.
- `FrameCompare.fx` compile in ReShade 6.8.
- D3D11 native Feature-18 runtime test.
- D3D12 state/ordering test in at least one native RenoDX/DLSS5 title.
- Feeder path test with `CaptureMode=2`.
- ShaderToggler hidden-HUD test.
- OBS/game-capture test of Freeze and Sweep.
- Label preview live-positioning test, including preview before a valid Before/After pair exists.

The GitHub Actions workflow is included specifically to perform the first Windows compile without requiring a local development setup.
