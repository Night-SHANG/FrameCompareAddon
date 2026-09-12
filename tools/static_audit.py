from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
REQUIRED = (
    "CMakeLists.txt",
    "FrameCompare.ini.example",
    "src/addon_entry.cpp",
    "src/config/ini_document.hpp",
    "src/config/ini_document.cpp",
    "src/config/settings_codec.hpp",
    "src/config/settings_encode.cpp",
    "src/config/settings_decode.cpp",
    "src/config/config_runtime.hpp",
    "src/config/config_runtime.cpp",
    "src/control/split_motion.hpp",
    "src/control/split_motion.cpp",
    "src/core/frame_pair.hpp",
    "src/core/frame_pair.cpp",
    "src/core/capture_cycle.hpp",
    "src/core/capture_cycle.cpp",
    "src/hud/indicator_model.hpp",
    "src/hud/indicator_model.cpp",
    "src/hud/indicator_panel.hpp",
    "src/hud/indicator_panel.cpp",
    "src/hud/indicator_overlay.hpp",
    "src/hud/indicator_overlay.cpp",
    "src/capture/reshade_capture.hpp",
    "src/capture/reshade_capture.cpp",
    "src/input/hotkeys.hpp",
    "src/input/hotkeys.cpp",
    "src/render/compositor.hpp",
    "src/render/compositor.cpp",
    "src/render/compositor_params.hpp",
    "src/render/compositor_params.cpp",
    "src/ui/label_layout.hpp",
    "src/ui/label_layout.cpp",
    "src/ui/label_overlay.hpp",
    "src/ui/label_overlay.cpp",
    "src/ui/parameter_widgets.hpp",
    "src/ui/parameter_widgets.cpp",
    "src/ui/i18n/localization.hpp",
    "src/ui/i18n/localization.cpp",
    "src/ui/pages/panel_pages.hpp",
    "src/ui/pages/panel_pages.cpp",
    "src/ui/panel.hpp",
    "src/ui/panel.cpp",
    "tests/label_layout_tests.cpp",
    "tests/split_motion_tests.cpp",
    "tests/ini_document_tests.cpp",
    "tests/indicator_model_tests.cpp",
    "tests/localization_tests.cpp",
    "shaders/FrameCompare.fx",
    ".github/workflows/build.yml",
)


def fail(message: str) -> None:
    print(f"AUDIT FAILED: {message}", file=sys.stderr)
    raise SystemExit(1)


def require_include_before(path: str, first: str, second: str) -> None:
    source = (ROOT / path).read_text(encoding="utf-8")
    first_index = source.find(first)
    second_index = source.find(second)
    if first_index < 0 or second_index < 0 or first_index > second_index:
        fail(f"{path} must include {first} before {second}")


for relative in REQUIRED:
    if not (ROOT / relative).is_file():
        fail(f"missing {relative}")

cmake_source = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
if "set(CMAKE_MSVC_RUNTIME_LIBRARY" not in cmake_source:
    fail("CMake must select one MSVC runtime library for every target")

include_order_requirements = (
    ("src/addon_entry.cpp", "#include <imgui.h>", '#include "capture/reshade_capture.hpp"'),
    ("src/ui/panel.cpp", "#include <imgui.h>", '#include "ui/panel.hpp"'),
    ("src/ui/label_overlay.cpp", "#include <imgui.h>", '#include "ui/label_overlay.hpp"'),
    ("src/input/hotkeys.cpp", "#include <imgui.h>", "#include <reshade.hpp>"),
    ("src/ui/parameter_widgets.cpp", "#include <imgui.h>", "#include <reshade.hpp>"),
    ("src/hud/indicator_overlay.cpp", "#include <imgui.h>", '#include "hud/indicator_overlay.hpp"'),
    ("src/hud/indicator_panel.cpp", "#include <imgui.h>", "#include <reshade.hpp>"),
)
for requirement in include_order_requirements:
    require_include_before(*requirement)

for folder in (ROOT / "src").iterdir():
    if not folder.is_dir():
        continue
    direct_files = [item for item in folder.iterdir() if item.is_file()]
    if len(direct_files) > 8:
        fail(f"{folder.relative_to(ROOT)} has {len(direct_files)} direct files")

capture_source = (ROOT / "src/capture/reshade_capture.cpp").read_text(
    encoding="utf-8"
)
if "CaptureProvenance::pre_reshade_fx" not in capture_source:
    fail("generic Before capture lacks explicit pre-ReShade provenance")

panel_source = (ROOT / "src/ui/panel.cpp").read_text(encoding="utf-8")
pages_source = (ROOT / "src/ui/pages/panel_pages.cpp").read_text(encoding="utf-8")
localization_source = (ROOT / "src/ui/i18n/localization.cpp").read_text(
    encoding="utf-8"
)
if "vanilla_warning" not in pages_source:
    fail("panel does not disclose the generic Before limitation")
if "mirror_left_to_right" not in pages_source or "mirror_right_to_left" not in pages_source:
    fail("panel is missing two-way label position mirroring")
if panel_source.count("BeginTabItem") != 6:
    fail("panel must expose exactly six top-level setting tabs")
indicator_panel_source = (ROOT / "src/hud/indicator_panel.cpp").read_text(
    encoding="utf-8"
)
if "###status-item" not in indicator_panel_source:
    fail("editable indicator titles must keep a stable ImGui ID")
for source in (panel_source, pages_source, indicator_panel_source):
    if " / " in source:
        fail("setting pages must not mix Chinese and English labels")

entry_source = (ROOT / "src/addon_entry.cpp").read_text(encoding="utf-8")
if 'register_overlay("OSD", framecompare::ui::draw_labels)' not in entry_source:
    fail("persistent labels are not registered with the ReShade OSD")
if "should_capture_pair(frozen, pair_ready)" not in entry_source:
    fail("freeze does not preserve a ready capture pair")

label_header = (ROOT / "src/ui/label_layout.hpp").read_text(encoding="utf-8")
if "'O', 'F', 'F'" not in label_header or "'O', 'N'" not in label_header:
    fail("persistent labels must default to OFF and ON")

hotkey_header = (ROOT / "src/input/hotkeys.hpp").read_text(encoding="utf-8")
if "ImGuiKeyChord" not in hotkey_header or "ImGuiKey_LeftArrow" not in hotkey_header:
    fail("hotkeys must use direct named-key chords rather than numeric VK input")
for required_binding in (
    "focus_left",
    "focus_right",
    "toggle_comparison",
    "toggle_display_mode",
    "toggle_border",
):
    if required_binding not in hotkey_header:
        fail(f"hotkeys are missing {required_binding}")

config_entry_requirements = (
    "framecompare::config::initialize(addon_module)",
    "framecompare::config::shutdown()",
)
for requirement in config_entry_requirements:
    if requirement not in entry_source:
        fail(f"add-on lifecycle is missing {requirement}")

config_runtime_source = (ROOT / "src/config/config_runtime.cpp").read_text(
    encoding="utf-8"
)
for requirement in ("FrameCompare.ini", "0.75f", "save_now", "reload_now"):
    if requirement not in config_runtime_source:
        fail(f"configuration runtime is missing {requirement}")

indicator_overlay_source = (ROOT / "src/hud/indicator_overlay.cpp").read_text(
    encoding="utf-8"
)
if "get_effects_state" not in indicator_overlay_source:
    fail("ReShade HUD source must read the actual effects state")

if "FrameCompare.ini.example" not in cmake_source:
    fail("portable INI example is not included in the build artifact")

motion_source = (ROOT / "src/control/split_motion.cpp").read_text(
    encoding="utf-8"
)
if "position = 0.5f" not in motion_source:
    fail("explicit autosweep stop must restore the split to the center")

label_overlay_source = (ROOT / "src/ui/label_overlay.cpp").read_text(
    encoding="utf-8"
)
if "PushClipRect" not in label_overlay_source or "PopClipRect" not in label_overlay_source:
    fail("persistent labels must be clipped to their current image regions")

widget_source = (ROOT / "src/ui/parameter_widgets.cpp").read_text(
    encoding="utf-8"
)
for required_widget in ("SliderFloat", "InputFloat", "i18n::TextId::reset"):
    if required_widget not in widget_source:
        fail("numeric settings must provide slider, number input and reset")

for requirement in (".addon32", ".addon64", "CMAKE_SIZEOF_VOID_P"):
    if requirement not in cmake_source:
        fail(f"dual-architecture CMake packaging is missing {requirement}")

workflow_source = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
for requirement in ("Win32", "x64", '"v*.*.*"', "contents: write",
                    "needs: windows", "gh release create", "SHA256SUMS.txt"):
    if requirement not in workflow_source:
        fail(f"release workflow is missing {requirement}")

if 'document.set("UI", "Language"' not in (
        ROOT / "src/config/settings_encode.cpp").read_text(encoding="utf-8"):
    fail("interface language is not persisted in FrameCompare.ini")

shader_source = (ROOT / "shaders/FrameCompare.fx").read_text(encoding="utf-8")
if "FRAMECOMPARE_BEFORE" not in shader_source or "FRAMECOMPARE_AFTER" not in shader_source:
    fail("shader is missing Before/After bindings")
if "center_focus" not in shader_source or "p1.w" not in shader_source:
    fail("center-remap shader is missing the adjustable source focus")

compositor_header = (ROOT / "src/render/compositor_params.hpp").read_text(
    encoding="utf-8"
)
if compositor_header.count("center_focus") < 2:
    fail("center focus must exist in settings and uploaded shader parameters")

print("static_audit: PASS")
