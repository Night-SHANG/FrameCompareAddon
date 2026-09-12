from pathlib import Path
from typing import Callable


REQUIRED = (
    "src/integrations/dlss5/model.hpp",
    "src/integrations/dlss5/model.cpp",
    "src/integrations/dlss5/runtime.hpp",
    "src/integrations/dlss5/runtime.cpp",
    "src/integrations/dlss5/ngx_abi.hpp",
    "src/integrations/dlss5/hook_manager.hpp",
    "src/integrations/dlss5/hook_manager.cpp",
    "src/integrations/dlss5/gpu/d3d11_copy.hpp",
    "src/integrations/dlss5/gpu/d3d11_copy.cpp",
    "src/integrations/dlss5/gpu/d3d12_copy.hpp",
    "src/integrations/dlss5/gpu/d3d12_copy.cpp",
    "tests/dlss5_model_tests.cpp",
    "THIRD_PARTY.md",
)


def audit_dlss5(root: Path, fail: Callable[[str], None]) -> None:
    for relative in REQUIRED:
        if not (root / relative).is_file():
            fail(f"missing DLSS5 integration file {relative}")

    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    for value in (
        "minhook", "v1.3.4", "d3d11", "d3d12",
        "00-nvngx.dll-FrameCompare-v2", "THIRD_PARTY.md",
    ):
        if value not in cmake:
            fail(f"DLSS5 CMake integration is missing {value}")

    entry = (root / "src/addon_entry.cpp").read_text(encoding="utf-8")
    for value in ("start_hooks()", "stop_hooks()", "publish_settings"):
        if value not in entry:
            fail(f"DLSS5 lifecycle is missing {value}")
    if "settings.enabled && settings.dlss5_before" not in entry:
        fail("DLSS5 activation is not derived from the authoritative settings")
    if "DisplayMode::same_coordinate_wipe" not in entry:
        fail("DLSS5 copy must be disabled for center-remap mode")

    encode = (root / "src/config/settings_encode.cpp").read_text(
        encoding="utf-8"
    )
    decode = (root / "src/config/settings_decode.cpp").read_text(
        encoding="utf-8"
    )
    if '"DLSS5", "Enabled"' not in encode or '"DLSS5", "Enabled"' not in decode:
        fail("DLSS5 checkbox is not persisted in both directions")

    d3d11 = (root / "src/integrations/dlss5/gpu/d3d11_copy.cpp").read_text(
        encoding="utf-8"
    )
    if "CopySubresourceRegion" not in d3d11:
        fail("D3D11 DLSS5 backend has no region copy")

    d3d12 = (root / "src/integrations/dlss5/gpu/d3d12_copy.cpp").read_text(
        encoding="utf-8"
    )
    for value in (
        "D3D12_RESOURCE_BARRIER_TYPE_UAV",
        "D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE",
        "D3D12_RESOURCE_STATE_UNORDERED_ACCESS",
        "D3D12_RESOURCE_STATE_COPY_SOURCE",
        "D3D12_RESOURCE_STATE_COPY_DEST",
        "CopyTextureRegion",
    ):
        if value not in d3d12:
            fail(f"D3D12 DLSS5 backend is missing {value}")

    integration = "\n".join(
        path.read_text(encoding="utf-8")
        for path in (root / "src/integrations/dlss5").rglob("*.cpp")
    )
    for forbidden in ("Map(", "ReadFromSubresource(", "Dispatch("):
        if forbidden in integration:
            fail(f"DLSS5 integration must not use {forbidden}")

    panel = (root / "src/ui/pages/panel_pages.cpp").read_text(encoding="utf-8")
    for value in ("dlss5_before", "dlss5_center_unsupported", "hook_snapshot"):
        if value not in panel:
            fail(f"DLSS5 comparison UI is missing {value}")

    for folder in (root / "src").rglob("*"):
        if folder.is_dir():
            count = sum(1 for item in folder.iterdir() if item.is_file())
            if count > 8:
                fail(f"{folder.relative_to(root)} has {count} direct files")
