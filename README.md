# FrameCompare v1.1

FrameCompare is a 64-bit ReShade Add-on for recording true Before/After-style comparisons when ordinary `.fx` split-screen shaders are too late in the pipeline.

It captures a **Before** image, lets the normal enhancement chain continue, captures the **After** image after ReShade effects, and performs a final split/wipe composite. It is designed for DLSS5/RenoDX/ReShade comparison workflows, but its generic capture modes can also be used without DLSS.

## Main features

- Add-on-level Before/After capture.
- Native NGX D3D11/D3D12 interception for numeric NGX Feature 18, capturing its `Color` input immediately before `EvaluateFeature`.
- `Before ReShade FX` fallback/capture mode for Feeder/effect-chain DLSS5 integration.
- Freeze both captured frames without pausing the game.
- Adjustable vertical divider, mouse drag in the ReShade overlay, short-step and hold-to-move hotkeys.
- Automatic sweep with speed control and optional ping-pong.
- Normal wipe and a `SplitScreenCR`-style center-remap display mode.
- Independent customizable Before/After labels (`OFF` / `ON` by default), X/Y position, font, size, opacity and outline.
- Persistent label-placement preview: both labels can stay visible for live X/Y/font/outline tuning even before a valid comparison pair exists.
- Portable `FrameCompare.ini` reusable across games.
- ReShade Add-ons settings UI and diagnostics.
- No runtime dependency on the old SplitScreenCR shader or `sMask.png`.

## Default controls

| Action | Default |
|---|---|
| Toggle comparison | F9 |
| Freeze / unfreeze captured pair | F10 |
| Start/stop auto sweep | F11 |
| Divider left/right | Left / Right |
| Full Before | Home |
| Full After | End |

A short Left/Right press moves by `Divider.Step`. Holding after `Divider.HoldDelay` moves continuously at `Divider.MoveSpeed`.

## Capture modes

`CaptureMode=0` — **Auto**. Saves a same-frame Before-ReShade fallback, then replaces it with a fresh usable NGX Feature-18 pre-Evaluate capture when one exists.

`CaptureMode=1` — **NGX Feature 18 strict**. Intended for native DLSS5/RenoDX paths that actually dispatch the target feature through D3D11/D3D12 NGX. No generic fallback is substituted when the native capture is unavailable.

`CaptureMode=2` — **Before ReShade FX**. Recommended when DLSS5 is injected as a ReShade Feeder/effect-chain stage and the wanted original image is the buffer immediately before ReShade effects.

`CaptureMode=3` — **Application Present**. Generic ordering-dependent mode. It is provided for compatibility/testing, not as a guarantee of a pre-DLSS image.

## Important meaning of “Before”

For native Feature-18 capture, FrameCompare stores the `Color` resource submitted to that NGX evaluation. That is the real pre-Evaluate input resource, but it may be an internal/render-resolution image. A universal add-on cannot guarantee that this resource is mathematically identical to the final Present image produced by running the game with every DLSS5/RenoDX component completely removed.

For `Before ReShade FX`, the image is exactly the current ReShade effect target before the ReShade preset runs. If another add-on already changed the image earlier, that earlier change is naturally present.

The **After** image is captured at `reshade_finish_effects`, so it includes the current ReShade effect chain and anything already applied before that point. An unrelated add-on that deliberately draws even later than this event can still modify the finally presented frame afterward.

## Label placement preview

Enable **Label placement preview (always show both)** in the Add-ons page to tune label layout before recording. In preview mode:

- Both Before and After labels are forced visible at the same time.
- X/Y, font size, opacity and outline changes update live.
- If a valid comparison pair exists, the current comparison remains as the background while both labels ignore divider clipping.
- If no valid pair exists, FrameCompare snapshots the current post-effects target as a neutral background and still renders both labels.
- The preview is independent of the normal `Show labels` switch so it can be used purely as a layout/debug tool.

Disable preview before normal recording; normal mode restores region clipping so each label only appears in the side it describes. The option is stored as `Labels.Preview` in `FrameCompare.ini`.

## ShaderToggler / hidden HUD

FrameCompare never replays application draw calls. It only copies resources that already exist. Therefore ShaderToggler-blocked HUD draw calls are not deliberately restored by FrameCompare. This is the required behavior for clean comparison recording. Exact visual content still depends on the selected capture point: an NGX input may naturally be scene-only, while a later generic capture can contain whatever the game had already rendered at that stage.

## Build

Requirements for local build:

- Windows x64
- Visual Studio 2022 with Desktop development with C++ and a Windows SDK
- CMake 3.24+
- Git/network access for default dependency fetching, unless local ReShade/MinHook roots are supplied

Run:

```bat
build_vs2022.bat
```

Or:

```bat
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

Output package:

```text
build\Release\package\
  00-FrameCompare.addon64
  FrameCompare.fx
  FrameCompare.ini.example
  README.md
```

### GitHub Actions

Push/commit the repository to the `main` or `master` branch and GitHub Actions starts the build automatically. You can also rebuild manually with **Actions → Build FrameCompare → Run workflow**. The workflow builds on `windows-2022` and uploads `FrameCompare-Windows-x64` containing the add-on, shader, example INI, Chinese installation guide and SHA-256 file.

## Installation

See `docs/INSTALL_CN.md` for the recommended ReShade 6.8 Add-on installation and mode selection.

The complete Chinese feature specification and acceptance checklist is in `docs/FUNCTIONS_CN.md`.

## Architecture

```text
Game rendering / ShaderToggler blocking
        │
        ├─ Native path: NGX Feature 18 Color ──> FrameCompare Before snapshot
        │
        └─ Generic path: ReShade begin effects ─> Before fallback
                                             │
                                      DLSS/RenoDX/ReShade
                                             │
                                  ReShade finish effects
                                             │
                                          After
                                             │
                      FrameCompareComposite (late explicit technique)
                                             │
                                          Present
```

`FrameCompare.fx` is intentionally disabled as a normal preset technique. The add-on binds the Before/After/parameter/label textures and explicitly renders only `FrameCompareComposite` at the comparison stage.

## Current platform scope

- ReShade Add-on API 20+ (targeted at ReShade 6.8.x).
- Add-on binary: Windows x64 only.
- Native NGX Feature-18 interception: D3D11 + D3D12.
- Vulkan/OpenGL/D3D9/10 may still use generic ReShade capture modes if the ReShade runtime supports the required resource operations, but there is no native NGX Vulkan Feature-18 hook in v1.1.

## Safety / compatibility notes

- `NGX.D3D12ColorState` is configurable because D3D12 does not expose a universal query for the current state of an arbitrary application resource. The default is `NON_PIXEL_SHADER_RESOURCE (0x40)`.
- Cross-device/cross-API NGX shared-resource import is **off by default**. A shared handle alone does not synchronize GPU queues. The opt-in exists only for diagnostics/known bridges.
- The output file is named `00-FrameCompare.addon64` to improve the chance that its NGX hooks are installed early. Add-on/hook ordering is still not universally enforceable.
- If Feature 18 was created before FrameCompare installed its CreateFeature hook, strict mode intentionally does not guess that an unknown handle is Feature 18.
- External recording (OBS/game capture) is the intended workflow. ReShade's own screenshot timing may occur before the final explicit composite in some configurations.

## Source and licensing

FrameCompare's original source is MIT licensed. ReShade and MinHook are fetched as build dependencies under their respective licenses. NVIDIA SDK headers are **not** redistributed; `NgxAbi.hpp` contains only the minimal independently written ABI declarations needed to observe existing host calls. See `THIRD_PARTY.md`.
