from pathlib import Path
from typing import Callable


def audit_release(root: Path, fail: Callable[[str], None]) -> None:
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    for requirement in (".addon32", ".addon64", "CMAKE_SIZEOF_VOID_P"):
        if requirement not in cmake:
            fail(f"dual-architecture CMake packaging is missing {requirement}")

    workflow = (root / ".github/workflows/build.yml").read_text(
        encoding="utf-8"
    )
    if workflow.count("uses: actions/checkout@v4") < 2:
        fail("build and release jobs must each checkout the repository")
    requirements = (
        "Win32", "x64", '"v*.*.*"', "contents: write", "needs: windows",
        "gh release create", "SHA256SUMS.txt",
    )
    for requirement in requirements:
        if requirement not in workflow:
            fail(f"release workflow is missing {requirement}")

    settings = (root / "src/config/settings_encode.cpp").read_text(
        encoding="utf-8"
    )
    if 'document.set("UI", "Language"' not in settings:
        fail("interface language is not persisted in FrameCompare.ini")

    shader = (root / "shaders/FrameCompare.fx").read_text(encoding="utf-8")
    if "FRAMECOMPARE_BEFORE" not in shader or "FRAMECOMPARE_AFTER" not in shader:
        fail("shader is missing Before/After bindings")
    if "center_focus" not in shader or "p1.w" not in shader:
        fail("center-remap shader is missing the adjustable source focus")

    compositor = (root / "src/render/compositor_params.hpp").read_text(
        encoding="utf-8"
    )
    if compositor.count("center_focus") < 2:
        fail("center focus must exist in settings and shader parameters")
