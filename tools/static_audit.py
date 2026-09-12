from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
REQUIRED = (
    "CMakeLists.txt",
    "src/addon_entry.cpp",
    "src/core/frame_pair.hpp",
    "src/core/frame_pair.cpp",
    "src/core/capture_cycle.hpp",
    "src/core/capture_cycle.cpp",
    "src/capture/reshade_capture.hpp",
    "src/capture/reshade_capture.cpp",
    "src/render/compositor.hpp",
    "src/render/compositor.cpp",
    "src/render/compositor_params.hpp",
    "src/render/compositor_params.cpp",
    "src/ui/panel.hpp",
    "src/ui/panel.cpp",
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

require_include_before(
    "src/addon_entry.cpp",
    "#include <imgui.h>",
    '#include "capture/reshade_capture.hpp"',
)
require_include_before(
    "src/ui/panel.cpp",
    "#include <imgui.h>",
    '#include "ui/panel.hpp"',
)

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
if "not verified Vanilla" not in panel_source:
    fail("panel does not disclose the generic Before limitation")

shader_source = (ROOT / "shaders/FrameCompare.fx").read_text(encoding="utf-8")
if "FRAMECOMPARE_BEFORE" not in shader_source or "FRAMECOMPARE_AFTER" not in shader_source:
    fail("shader is missing Before/After bindings")

print("static_audit: PASS")
