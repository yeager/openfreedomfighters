# DLSS integration plan

## Decision

Modern+ targets NVIDIA DLSS 4.5 because it is the newest generation currently
described by NVIDIA's public [DLSS developer page](https://developer.nvidia.com/rtx/dlss).
The first integration target is Super Resolution. Ray Reconstruction is useful
only if OpenFreedomFighters later gains a ray-traced lighting path, and Frame
Generation remains independently selectable and later than a stable native
renderer. DLSS is never required to run Modern+.

DLSS 5 is not treated as a current product or API requirement. A future upgrade
requires official NVIDIA documentation, an obtainable SDK, explicit native
platform support, compatible redistribution terms, and image-quality validation.

## Renderer contract

The planned engine-owned vendor-neutral temporal-upscaler interface will accept:

- jittered render-resolution color;
- linear depth with the projection convention recorded explicitly;
- per-pixel motion vectors with scale and direction metadata;
- exposure or pre-exposure data;
- an optional reactive/transparency mask;
- render and output dimensions, jitter offset, and frame delta;
- camera reset, cut, resize, and history-invalidation flags; and
- an output-resolution destination that is complete before UI composition.

No user-selectable temporal adapter or resolver is currently exposed. The SDL
Modern path is spatial-only: fixed render scaling followed by linear output
scaling. Native-resolution, portable temporal, and DLSS adapters are future
implementations of this contract. A future FSR adapter may use the same contract
only after its official SDK and platform requirements are validated. Simulation,
input sampling, audio, saves, and replay state never depend on the selected
adapter or on generated presentation frames.

## Platform and packaging policy

At startup the renderer checks the active graphics API, GPU vendor and capability,
driver requirements, SDK availability, and feature support. It exposes DLSS only
when all checks succeed. Unsupported NVIDIA hardware, AMD, Intel, Apple, and Steam
Deck devices retain the same internal-resolution controls through the current
spatial path. The settings file stores intent rather than assuming that a
previously used backend remains available.

Only binaries that NVIDIA explicitly permits the project to redistribute may be
packaged. Proprietary SDK source, developer credentials, sample assets, and
non-redistributable files must not enter the repository, CI artifacts, or releases.
The optional adapter must fail closed and produce a useful diagnostic before
falling back. NVIDIA's open-source
[Streamline framework](https://github.com/NVIDIA-RTX/Streamline) may inform the
adapter boundary, but its presence does not prove that every plugin or native
target is available or redistributable.

## Delivery order

1. Ship correct native rendering, depth, exposure, and resolution-independent UI.
2. Add deterministic jitter and validated motion vectors for rigid and animated
   geometry, particles, transparencies, and camera cuts.
3. Establish the portable temporal baseline and reference captures.
4. Integrate DLSS 4.5 Super Resolution behind the same contract, beginning with
   the first-class Windows x64/D3D12 target and then each additional native target
   explicitly supported by NVIDIA's SDK.
5. Evaluate an optional FSR adapter independently against AMD's official supported
   APIs and redistribution terms; do not expose or label it before the adapter,
   required frame inputs, and quality validation exist.
6. Compare disocclusion, foliage, particles, HUD edges, thin geometry, camera cuts,
   and ultrawide output against native and portable paths.
7. Add latency instrumentation and only then evaluate Frame Generation.
8. Re-evaluate later DLSS generations without changing the public renderer
   contract or removing fallbacks.

## Acceptance gates

- DLSS reports the actual loaded version and cannot be mislabeled.
- Original mode remains reference-compatible and does not silently enable DLSS.
- UI and subtitles are composed at output resolution after upscaling.
- Camera cuts, scene loads, resolution changes, and pause/resume invalidate history.
- Missing libraries or unsupported drivers fall back without a crash or black frame.
- DLSS and portable rendering produce identical authoritative simulation hashes.
- Packaged binaries pass the retail-content audit, license audit, and Gitleaks.
- Image-quality captures and frametime traces cover every supported quality preset.
