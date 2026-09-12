# FrameCompare v2

FrameCompare v2 is a ReShade add-on for same-frame realtime Before/After comparison. Development starts from a small verifiable capture core instead of the static screenshot workflow used by the old v1.3 reference.

## Current behavior

- Captures the render target immediately before and after the ReShade effect chain.
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

Default controls are `Ctrl+Left` and `Ctrl+Right` for manual movement, `Alt+Left` and `Alt+Right` for center focus, `Ctrl+F7` to enable or disable comparison, `Ctrl+F8` to start or stop the selected automatic sweep, `Ctrl+F9` to freeze or resume capture, `Ctrl+F10` to switch display mode, and `Ctrl+F11` to show or hide the border. Explicitly stopping a sweep returns the split to the center. Click a binding in the add-on panel and press a new key or chord to replace it; use the adjacent clear button to unbind it.

For precise tuning, type a value in the number field beside a slider. Each row has its own reset button. Label positions can be mirrored in either direction, and comparison, motion, hotkeys and labels each have a section reset.

Center remap uses a `0.5` source focus by default. Lower values move both compared views toward the original image's left side, while higher values move them toward the right. Automatic sweep remains available in this mode, but the panel marks the combination as not recommended because center remap is intended for aligned subject comparison.

## Configuration and status indicators

`FrameCompare.ini` is created beside `00-FrameCompare-v2.addon64`. Stable changes are saved automatically after a short delay and again when the add-on unloads. The panel also provides Save now and Reload buttons. Copy this file with the add-on to reuse layout, controls and HUD entries in another game.

Each custom status entry can either read the actual ReShade effects state or track a shared external shortcut. The ReShade source follows `effect_runtime::get_effects_state()` and therefore does not need a duplicate `END` binding. A tracked shortcut is suitable for ShaderToggler groups and shader toggles such as HDR or RTGI, but starts from the configured initial state and cannot verify the other add-on's state if a key press is missed or the external state changes elsewhere.

The group and portable-INI concepts are inspired by the MIT-licensed [ShaderToggler](https://github.com/FransBouma/ShaderToggler). No shader interception code is copied; FrameCompare only observes configured shortcuts and draws its own OSD notifications.

The generic source is deliberately labeled `Pre-ReShade FX`. It is not claimed to be verified Vanilla because DLSS, RenoDX or game-specific processing may already have changed the frame.

## Build

GitHub Actions is the authoritative build environment. The workflow uses Windows Server 2022, Visual Studio, CMake and ReShade API v6.8.0.

For core-only local tests with CMake:

```powershell
cmake -S . -B build-tests -A x64 -DFRAMECOMPARE_BUILD_ADDON=OFF
cmake --build build-tests --config Release
ctest --test-dir build-tests -C Release --output-on-failure
```

The uploaded artifact contains:

```text
00-FrameCompare-v2.addon64
FrameCompare.ini.example
reshade-shaders/Shaders/FrameCompare.fx
```

The Phase 1 realtime split was accepted in-game on 2026-09-12. Persistent labels, motion, automatic sweep, direct hotkey capture, freeze and precision controls are implemented in Phase 2. INI persistence and customizable multi-entry status HUD are implemented in Phase 3. These features require in-game validation with the latest artifact. Provider-specific verified Vanilla capture remains later work.
