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
- Pairs every numeric slider with direct number entry and a per-value reset button.
- Mirrors either label position to the opposite side and offers section-level default resets.
- Disables capture and composition when the add-on switch is off.

Default controls are `Ctrl+Left` and `Ctrl+Right` for manual movement, `Ctrl+F7` to enable or disable comparison, `Ctrl+F8` to start or stop the selected automatic sweep, `Ctrl+F9` to freeze or resume capture, `Ctrl+F10` to switch display mode, and `Ctrl+F11` to show or hide the border. Explicitly stopping a sweep returns the split to the center. Click a binding in the add-on panel and press a new key or chord to replace it; use the adjacent clear button to unbind it.

For precise tuning, type a value in the number field beside a slider. Each row has its own reset button. Label positions can be mirrored in either direction, and comparison, motion, hotkeys and labels each have a section reset.

Center remap uses a `0.5` source focus by default. Lower values move both compared views toward the original image's left side, while higher values move them toward the right. Automatic sweep remains available in this mode, but the panel marks the combination as not recommended because center remap is intended for aligned subject comparison.

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
reshade-shaders/Shaders/FrameCompare.fx
```

The Phase 1 realtime split was accepted in-game on 2026-09-12. Persistent labels, motion, automatic sweep, direct hotkey capture, freeze and precision controls are implemented in Phase 2 and require validation with the next GitHub Actions artifact. INI persistence, customizable multi-entry HUD and provider-specific verified Vanilla capture remain later work.
