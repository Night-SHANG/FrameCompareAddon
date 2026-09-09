# Third-party components

FrameCompare source itself is licensed under MIT (`LICENSE`).

## ReShade Add-on API

Project: ReShade by Patrick Mours and contributors  
Repository: https://github.com/crosire/reshade  
License identifiers in current public API headers: BSD-3-Clause OR MIT.

The build fetches ReShade headers and the matching Dear ImGui submodule. ReShade source is not copied into this repository.

## Dear ImGui

Project: Dear ImGui by Omar Cornut and contributors  
Repository: https://github.com/ocornut/imgui  
License: MIT.

The matching version is obtained through ReShade's `deps/imgui` submodule.

## MinHook

Project: MinHook by Tsuda Kageyu and contributors  
Repository: https://github.com/TsudaKageyu/minhook  
Version requested by CMake: v1.3.4  
License: 2-clause BSD.

MinHook is fetched at build time; its source is not copied into this repository.

## NVIDIA NGX interface

Public reference repository: https://github.com/NVIDIA/DLSS

NVIDIA's public NGX SDK headers are proprietary (`LicenseRef-NvidiaProprietary`) and are **not redistributed** by FrameCompare.

`src/NgxAbi.hpp` contains only a small independently written declaration of the ABI signatures needed to observe calls already made by the host process: opaque handle, parameter virtual interface layout, callback types, numeric target feature and the `Color` parameter name. No NVIDIA implementation code is included.

Important: NVIDIA's current public enum names numeric feature 18 `Reserved18`; FrameCompare's use of that number for the tested DLSS5 ecosystem is not an official public NVIDIA naming guarantee.

## Legacy shader references

The user-provided `SplitScreenCR.fx` and `sMask.png` are not distributed with this project because their redistribution license was not established. See `references/README.md`.
