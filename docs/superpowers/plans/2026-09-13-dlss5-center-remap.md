# DLSS5 center-remap implementation plan

## Outcome

When `DLSS5 Before` and center remap are both enabled, the Before side samples a
stable full-frame copy of `DLSSNR.Color`. The After side remains the existing
Post-ReShade capture. The same-coordinate path and every generic FrameCompare
path retain their current behavior.

## Boundaries

- `integrations/dlss5/center`: owns the native D3D11/D3D12 capture bridge,
  texture generation and fail-open status.
- `capture`: consumes one new DLSS input generation when it creates a frame
  pair, then copies it into a normal ReShade-owned texture so freeze remains
  stable.
- `render`: selects the Before texture from the capture provenance. Existing
  center UV math is unchanged.
- `ui/i18n`: reports active, waiting and fallback states without hard-coded
  user-facing text.

## Native resource path

- D3D11 NGX: copy the complete Color resource into a D3D11 staging texture on
  the supplied immediate context.
- D3D12 NGX in a D3D11 ReShade runtime: create a shared texture on the same
  adapter, append the complete Color copy to the supplied DLSS command list,
  and open the shared resource on D3D11.
- The copy is recorded after successful NGX evaluation on the same command
  list that produces the output RenoDX must complete before exposing the frame
  to the D3D11 ReShade runtime.
- Any device, adapter, format, dimension or sharing failure invalidates only
  the DLSS center source. Capture then uses the generic Pre-ReShade source.

## Frame identity and lifetime

- Each successfully recorded full-frame copy publishes a monotonically
  increasing generation.
- A FrameCompare capture cycle consumes a generation at most once.
- The consumed resource is copied into `RuntimeCaptureState::dlss_before`.
  This owned copy is the shader source and remains stable while capture is
  frozen.
- Runtime destruction waits for the ReShade queue, releases owned captures and
  detaches the bridge. Add-on shutdown stops hooks before final bridge cleanup.

## Verification

1. Pure tests cover mode selection, fresh-generation selection and fail-open
   fallback.
2. Existing tests prove unchanged generic and same-coordinate behavior.
3. Static Windows compilation covers both D3D11 and D3D12 native paths.
4. GitHub Actions builds x64 and x86 artifacts.
5. In-game validation checks center focus, freeze/unfreeze, mode switching,
   resolution changes and disabling DLSS5.

