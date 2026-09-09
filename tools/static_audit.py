#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
errors = []
notes = []

def text(rel):
    p = ROOT / rel
    if not p.exists():
        errors.append(f"missing file: {rel}")
        return ""
    return p.read_text(encoding="utf-8")

cpp = text("src/FrameCompare.cpp")
fx = text("shaders/FrameCompare.fx")
cmake = text("CMakeLists.txt")
rc = text("src/FrameCompare.rc")
ini = text("FrameCompare.ini.example")

checks = [
    ("project(FrameCompare VERSION 1.3.0", cmake, "CMake project version is not 1.3.0"),
    ('FILEVERSION 1,3,0,0', rc, "VERSIONINFO file version is not 1.3.0.0"),
    ('PRODUCTVERSION 1,3,0,0', rc, "VERSIONINFO product version is not 1.3.0.0"),
    ('capture_strategy::exact_snapshot_pair', cpp, "exact snapshot workflow missing"),
    ('capture_strategy::live_pipeline', cpp, "live pipeline workflow missing"),
    ('hud_state_source::reshade_effects_state', cpp, "actual ReShade Effects State HUD source missing"),
    ('hud_state_source::hotkey_pulse', cpp, "one-shot HUD message source missing"),
    ('runtime->get_effects_state()', cpp, "ReShade Effects State polling missing"),
    ('capture_pair_compatible', cpp, "Before/After compatibility guard missing"),
    ('FRAMECOMPARE_BEFORE', fx, "Before binding semantic missing in shader"),
    ('FRAMECOMPARE_AFTER', fx, "After binding semantic missing in shader"),
    ('FRAMECOMPARE_PARAMS', fx, "parameter binding semantic missing in shader"),
    ('technique FrameCompareComposite', fx, "compositor technique missing"),
    ('DisplayMode=1', ini, "SplitScreenCR-style mode is not the example default"),
]
for needle, haystack, msg in checks:
    if needle not in haystack:
        errors.append(msg)

for forbidden, where, msg in [
    ("application_present", cpp.lower(), "obsolete application_present capture mode remains in C++"),
    ("CreateFont", cpp, "legacy GDI font creation remains"),
    ("CreateDIBSection", cpp, "legacy GDI bitmap text path remains"),
    ("TextOut", cpp, "legacy GDI text rendering remains"),
    ("gdi32", cmake.lower(), "obsolete GDI32 linker dependency remains"),
]:
    if forbidden in where:
        errors.append(msg)

# Default full-frame shortcuts must not occupy Home/End, which are common ReShade keys.
if re.search(r'hk_full_before\s*\{\s*VK_(?:HOME|END)', cpp):
    errors.append("Full Before default still uses Home/End")
if re.search(r'hk_full_after\s*\{\s*VK_(?:HOME|END)', cpp):
    errors.append("Full After default still uses Home/End")

# Lightweight delimiter sanity after removing comments and quoted strings. This is not a compiler.
def strip_cpp_noise(s):
    s = re.sub(r'/\*.*?\*/', '', s, flags=re.S)
    s = re.sub(r'//[^\n]*', '', s)
    s = re.sub(r'R".*?"', '""', s, flags=re.S)
    s = re.sub(r'"(?:\\.|[^"\\])*"', '""', s)
    s = re.sub(r"'(?:\\.|[^'\\])*'", "''", s)
    return s

for rel, payload in [("src/FrameCompare.cpp", cpp), ("shaders/FrameCompare.fx", fx)]:
    cleaned = strip_cpp_noise(payload)
    for left, right in [("{", "}"), ("(", ")"), ("[", "]")]:
        if cleaned.count(left) != cleaned.count(right):
            errors.append(f"{rel}: unbalanced {left}{right} delimiters")

if "GIT_TAG main" in cmake:
    notes.append("ReShade dependency tracks main; build reproducibility depends on the checkout date unless FRAMECOMPARE_RESHADE_ROOT is supplied.")

if errors:
    print("FrameCompare static audit: FAILED")
    for e in errors:
        print(f"  ERROR: {e}")
    for n in notes:
        print(f"  NOTE: {n}")
    sys.exit(1)

print("FrameCompare static audit: PASS")
for n in notes:
    print(f"  NOTE: {n}")
print("  This is a source/static audit only; it does not replace an MSVC/ReShade runtime test.")
