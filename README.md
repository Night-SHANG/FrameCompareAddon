# FrameCompare v2

FrameCompare v2 is a ReShade add-on for same-frame realtime Before/After comparison. Development starts from a small verifiable capture core instead of the static screenshot workflow used by the old v1.3 reference.

## Current Phase 1 behavior

- Captures the render target immediately before and after the ReShade effect chain.
- Requires both captures to have the same runtime and effect-cycle token.
- Composites the pair entirely on the GPU.
- Supports a same-coordinate wipe and a SplitScreenCR-inspired center remap.
- Exposes split position, side order, border and display mode in the ReShade add-on panel.
- Disables capture and composition when the add-on switch is off.

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

This phase has not yet received in-game runtime acceptance. Freeze, automatic sweep, full hotkey capture, INI persistence and provider-specific verified Vanilla capture are later phases.
