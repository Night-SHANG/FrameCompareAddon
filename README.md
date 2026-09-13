# FrameCompare v2

FrameCompare v2 is a ReShade add-on for same-frame realtime Before/After comparison. Development starts from a small verifiable capture core instead of the static screenshot workflow used by the old v1.3 reference.

## Current behavior

- Captures the render target immediately before and after the ReShade effect chain.
- Optionally uses the pre-DLSS5 `DLSSNR.Color` image for Before after a
  successful D3D11/D3D12 DLSSNR Evaluate call.
- Keeps the original generic Pre-ReShade Before path when the DLSS5 option is
  disabled, unavailable or incompatible.
- Requires both captures to have the same runtime and effect-cycle token.
- Composites the pair entirely on the GPU.
- Supports a same-coordinate wipe and a SplitScreenCR-inspired center remap.
- Adjusts the shared source focus in center-remap mode for off-center subjects.
- Exposes split position, side order, border and display mode in the ReShade add-on panel.
- Draws persistent `OFF` / `ON` labels through the ReShade OSD while comparison is enabled.
- Allows custom Before/After text, left/right position, font size, opacity and outline style.
- Keeps semantic labels attached to Before/After when the side order is reversed.
- Clips each label to its current image region, so the sweep boundary gradually covers the label when that image disappears.
- Moves the split smoothly while configurable left/right chords are held.
- Supports left-to-right, right-to-left and ping-pong automatic sweeps.
- Restores the split to the center when an active sweep is explicitly stopped.
- Starts ping-pong sweeps at the center moving right, and returns smoothly to the center when stopped.
- Freezes the latest ready Before/After pair while keeping split motion and labels active.
- Captures hotkeys directly from key presses and shows readable chord names.
- Provides configurable hotkeys for comparison enable, display mode and border visibility.
- Moves center-remap focus with configurable `Alt+Left` / `Alt+Right` chords.
- Pairs every numeric slider with direct number entry and a per-value reset button.
- Mirrors either label position to the opposite side and offers section-level default resets.
- Automatically persists comparison, motion, labels, hotkeys and HUD entries in a portable `FrameCompare.ini` beside the add-on DLL.
- Supports multiple named status indicators with configurable ON/OFF text, position and display duration.
- Reads the real ReShade effects state directly, while clearly labeling generic ShaderToggler/HDR/RTGI shortcuts as locally tracked state.
- Disables capture and composition when the add-on switch is off.
- Organizes settings into six top tabs: Compare, Motion, Hotkeys, Labels, Status and Config.
- Defaults to Simplified Chinese and can switch the whole interface to English; the selected language persists in `FrameCompare.ini`.

Default controls are `Ctrl+Left` and `Ctrl+Right` for manual movement, `Alt+Left` and `Alt+Right` for center focus, `Ctrl+F7` to enable or disable comparison, `Ctrl+F8` to start or stop the selected automatic sweep, `Ctrl+F9` to freeze or resume capture, `Ctrl+F10` to switch display mode, and `Ctrl+F11` to show or hide the border. Explicitly stopping a sweep returns the split to the center. Click a binding in the add-on panel and press a new key or chord to replace it; use the adjacent clear button to unbind it.

For precise tuning, type a value in the number field beside a slider. Each row has its own reset button. Label positions can be mirrored in either direction, and comparison, motion, hotkeys and labels each have a section reset.

Center remap uses a `0.5` source focus by default. Lower values move both compared views toward the original image's left side, while higher values move them toward the right. The focus is mapped into the widest range that keeps both source windows inside the frame, preventing edge pixels from stretching into horizontal bands. Automatic sweep remains available in this mode, but the panel marks the combination as not recommended because center remap is intended for aligned subject comparison.

## Configuration and status indicators

`FrameCompare.ini` is created beside the add-on. Stable changes, including the interface language, are saved automatically after a short delay and again when the add-on unloads. The Config tab also provides Save now and Reload buttons. Copy this file with the add-on to reuse layout, controls and HUD entries in another game.

Each custom status entry can either read the actual ReShade effects state or track a shared external shortcut. The ReShade source follows `effect_runtime::get_effects_state()` and therefore does not need a duplicate `END` binding. A tracked shortcut is suitable for ShaderToggler groups and shader toggles such as HDR or RTGI, but starts from the configured initial state and cannot verify the other add-on's state if a key press is missed or the external state changes elsewhere.

The group and portable-INI concepts are inspired by the MIT-licensed [ShaderToggler](https://github.com/FransBouma/ShaderToggler). No shader interception code is copied; FrameCompare only observes configured shortcuts and draws its own OSD notifications.

The generic source is deliberately labeled `Pre-ReShade FX`. It is not claimed to be verified Vanilla because DLSS, RenoDX or game-specific processing may already have changed the frame.

## DLSS5-aware Before source

The Compare tab contains `DLSS5 处理前画面作为 Before`, disabled by default.
With it disabled, FrameCompare behaves like v2.0.0: Before is the existing
Pre-ReShade FX capture and After is Post-ReShade FX.

With it enabled in same-coordinate mode, FrameCompare observes the existing
`nvngx_dlssnr.dll` call. After DLSSNR finishes, it copies the current Before-side
region from `DLSSNR.Color` into `DLSSNR.Output`. The existing FrameCompare capture
then produces this comparison:

- Before side: game image before DLSS5 and before the ReShade preset.
- After side: DLSS5 output with the ReShade preset.

Side order, split position, labels, manual movement, sweep, freeze, border and
hotkeys continue to use the existing FrameCompare settings. In center-remap
mode, FrameCompare captures the complete `DLSSNR.Color` resource and copies it
into its own stable Before texture when a new comparison pair is created. The
existing center-focus UV calculation is then applied to that texture, so freeze
and unfreeze retain the same behavior as the generic center-remap path. When a
frame contains multiple DLSSNR passes, the bridge retains the first pass input
until FrameCompare consumes it; later passes cannot replace Before with an
already processed DLSS image.

The integration is fail-open. A missing module, missing parameter, incompatible
resource, unavailable shared bridge, existing detour or hook failure leaves the
original NGX result and generic FrameCompare path available. D3D12 same-coordinate
region copying was validated in game with DLSS5 still operational. The
D3D12-to-D3D11 center bridge was validated in game with one DLSSNR pass. The
first-pass latch for multi-pass DLSSNR and D3D11 still need in-game validation.

## Installation and DLSS5 test

Copy the package contents beside the game ReShade installation. Before testing
this integrated build, remove these older add-ons from the ReShade add-on folder:

- `00-FrameCompare-v2.addon64` or `00-FrameCompare-v2.addon32`
- the standalone `RenoDxNgxObserver` validator

Do not load either old file together with the integrated add-on because two
copies would compete for the same FrameCompare or NGX hooks. Keep the game's
normal RenoDX DLSS add-on and `nvngx_dlssnr.dll` in place.

Start the game, open ReShade's Add-ons page and expand FrameCompare v2. In the
Compare tab, verify that the DLSS5 module changes from `等待载入` to `已载入` and
that at least one D3D11 or D3D12 Evaluate hook shows `1`. Enable realtime
comparison, use same-coordinate mode, then check `DLSS5 处理前画面作为 Before`.
The matching API's applied counter should increase. `Last result: applied`
confirms that the pre-DLSS5 region was copied; skipped counters and the last
error explain why the generic path was used instead.

Switch to center-remap mode and leave the checkbox enabled. `中心捕获: 已就绪`
with an increasing generation confirms that complete input frames reach the
bridge. Move the center focus, freeze and unfreeze the pair, then switch between
display modes. `中心捕获: 回退中` means this frame is using the generic
Pre-ReShade Before and the bridge error line identifies the rejected condition.

## Build

GitHub Actions is the authoritative build environment. The workflow uses Windows Server 2022, Visual Studio, CMake and ReShade API v6.8.0. Every build verifies both x64 and Win32; version tags matching `v*.*.*` publish a GitHub Release only after both architectures pass.

For core-only local tests with CMake:

```powershell
cmake -S . -B build-tests -A x64 -DFRAMECOMPARE_BUILD_ADDON=OFF
cmake --build build-tests --config Release
ctest --test-dir build-tests -C Release --output-on-failure
```

The uploaded artifact contains:

```text
00-nvngx.dll-FrameCompare-v2.addon64 (x64 package)
00-nvngx.dll-FrameCompare-v2.addon32 (x86 package)
FrameCompare.ini.example
THIRD_PARTY.md
reshade-shaders/Shaders/FrameCompare.fx
```

The Phase 1 realtime split was accepted in-game on 2026-09-12. Persistent labels, motion, automatic sweep, direct hotkey capture, freeze and precision controls are implemented in Phase 2. INI persistence and customizable multi-entry status HUD are implemented in Phase 3. The standalone D3D12 DLSSNR validator proved that the in-place region copy can run while DLSS5 remains operational; this behavior is integrated behind the optional checkbox. Version 2.2 adds a complete-input center-remap bridge. The new bridge still requires in-game validation with the latest artifact.
