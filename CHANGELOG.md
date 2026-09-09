# Changelog

## 1.1.0 — 2026-09-08

- Added persistent label-placement preview mode for live X/Y/font/outline tuning.
- Preview can render both labels before a valid Before/After pair exists by using the current post-effects frame as a neutral background.
- Preview forces both labels visible and bypasses normal divider-region clipping; normal recording behavior is unchanged when preview is off.
- Added `Labels.Preview` to the portable INI and diagnostics.
- Clarified GitHub Actions auto-build behavior on pushes to `main`/`master`.

## 1.0.0 — 2026-09-07

- Initial complete source release.
- ReShade API 20+ add-on architecture.
- D3D11/D3D12 NGX numeric Feature-18 pre-Evaluate Color capture.
- Before-ReShade-FX and Present generic capture modes.
- Freeze, manual divider movement, hold-to-move, auto sweep and ping-pong.
- Normal wipe and SplitScreenCR-style center-remap presentation.
- Custom Before/After text labels with independent normalized X/Y positioning.
- Portable INI and ReShade Add-ons settings/diagnostics UI.
- GitHub Actions Windows x64 build workflow.
