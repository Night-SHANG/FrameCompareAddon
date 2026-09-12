# Phase 3 Reuse Audit

## ShaderToggler

- Source: <https://github.com/FransBouma/ShaderToggler>
- License: MIT.
- Useful model: named toggle groups, direct shortcut capture, and a portable INI beside the add-on.
- Adaptation: FrameCompare uses named OSD status entries that may share the same shortcut as a ShaderToggler group.
- Limitation: ShaderToggler does not expose a stable cross-add-on group-state API. FrameCompare therefore labels this source as tracked hotkey state and requires a configured initial state.
- Rejected scope: shader hunting, shader interception, and ShaderToggler's rendering hooks are unrelated to FrameCompare status notifications and are not copied.

## FrameCompare v1.3 reference

- License: MIT in the preserved reference folder.
- Useful model: `FrameCompare.ini`, debounced saving, indexed HUD sections, timed state messages, and direct ReShade effects-state queries.
- Adaptation: only the portable data and behavior concepts are retained.
- Rejected structure: the old implementation concentrates configuration, capture, rendering, input, HUD and UI in one large source file and is not reused as the v2 architecture.

## ReShade 6.8 API

- Source: <https://github.com/crosire/reshade/blob/v6.8.0/include/reshade_api.hpp>
- License: BSD-3-Clause.
- Adopted API: `effect_runtime::get_effects_state()` is the authoritative source for ReShade total-effects status.
