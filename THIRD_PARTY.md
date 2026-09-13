# Third-party components

## ReShade

- Repository: https://github.com/crosire/reshade
- Pinned version: `v6.8.0`
- License: BSD 3-Clause
- Usage: add-on API and matching Dear ImGui headers, downloaded during build.

## MinHook

- Repository: https://github.com/TsudaKageyu/minhook
- Pinned version: `v1.3.4`
- License: 2-Clause BSD
- Usage: hooks the loaded `nvngx_dlssnr.dll` Evaluate exports.

## DLSS5-Feeder

- Repository: https://github.com/jlrouzies-fr/DLSS5-Feeder
- License: MIT for repository-owned source.
- Usage: design reference for frame identity, GPU fences and delayed resource
  retirement. No source is copied.

## OptiScaler-DLSSNR-PreSR-Multipass

- Repository: https://github.com/wilsjo2/OptiScaler-DLSSNR-PreSR-Multipass
- License: GPL-3.0.
- Usage: read-only design reference for Pre-SR/Post-SR boundaries and submission
  epochs. No source is copied or linked.

## neural-upstream

- Repository: https://github.com/matiasLombo/neural-upstream
- License: MIT, Copyright (c) 2026 Matias Lombo
- Usage: research reference for discovering NGX providers and reading existing
  DLSSNR parameter resources without replacing them.

## DLSS5-Reshade-AIO

- Repository: https://github.com/kibblerz/DLSS5-Reshade-AIO
- License: Apache-2.0 for repository-owned source.
- Usage: reference for creating D3D11/D3D12 shared texture pairs in either
  direction. FrameCompare implements a smaller bridge scoped to its Before
  capture and does not include or link the AIO project.

## NVIDIA NGX names

This project declares only the minimal ABI shapes and public parameter/export
names needed to observe calls already made by the host process. It does not
redistribute NVIDIA SDK headers, libraries, models, or runtime DLLs.

## Historical local references

`FrameCompareAddon_v1.3_complete` is project-owned MIT reference code.
`SplitScreenCR.fx` has no confirmed redistribution license, so FrameCompare v2
independently implements only its observable center-remap behavior.
