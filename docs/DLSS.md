# DLSS integration plan

## Decision

The public cross-engine integration target remains DLSS 4.5 Super Resolution
through NVIDIA's official [DLSS developer resources](https://developer.nvidia.com/rtx/dlss)
and [DLSS SDK](https://github.com/NVIDIA/DLSS). Ray Reconstruction is useful
only if OpenFreedomFighters later gains a ray-traced lighting path, and Frame
Generation remains independently selectable and later than a stable native
renderer. DLSS is never required to run Modern+.

DLSS 5 is a released NVIDIA neural-rendering product. NVIDIA describes it as a
final, one-frame-in/one-frame-out rendering stage conditioned on the rendered
frame, engine motion vectors, temporal state, and developer art-direction
controls. NVIDIA reports availability in NBA 2K27 on GeForce RTX 50 Series.
NVIDIA also says DLSS 5 integration uses the Streamline framework. See NVIDIA's
[DLSS 5 developer overview](https://developer.nvidia.com/blog/whats-new-for-game-developers-dlss-5-with-3d-guided-neural-rendering-nvidia-ace-updates-and-new-rtx-kit-capabilities/),
[DLSS 5 research overview](https://research.nvidia.com/labs/adlr/DLSS5/), and
[launch announcement](https://www.nvidia.com/en-us/geforce/news/dlss-5-3d-guided-neural-rendering/).

As checked on 2026-10-30, the public Streamline 2.14.16 repository documents
DLSS Super Resolution, Ray Reconstruction, and Frame Generation, but its public
headers, guides, and source tree do not expose a DLSS 5 Neural Rendering feature
or integration contract. NVIDIA's product-level statement that DLSS 5 uses
Streamline is not enough to infer the feature API, required engine resources,
supported native APIs, or redistribution terms for a custom renderer. Keep DLSS
5 as a distinct implementation target and request/use NVIDIA's authorized
integration materials when available. Do not substitute community loaders,
leaked binaries, or DLSS Super Resolution and label them DLSS 5. See the
[official Streamline repository](https://github.com/NVIDIA-RTX/Streamline) and
[Streamline programming guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuide.md).

As checked on 2026-10-30, NVIDIA's public DLSS SDK release history lists
310.9.16 (released 2026-10-16), and NVIDIA identifies DLSS 4.5 as its current
Super Resolution integration target. Recheck the official
[release history](https://github.com/NVIDIA/DLSS/releases) before beginning an
adapter; SDK versions and redistribution requirements can change.

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

## External compatibility projects

[DLSS-NR on AMD](https://github.com/danielblnc/DLSS-NR-on-AMD) is not an
integration dependency or a supported runtime for OpenFreedomFighters. Its
published instructions describe a Windows 11, DirectX 12 mod loader for AMD
GPUs; Vulkan and Linux are listed as planned. The project also requires users
to supply NVIDIA DLSS DLLs. Its repository license prohibits redistribution,
commercial use, modification, and reverse engineering of that software.
Accordingly, OpenFreedomFighters must not bundle, link, copy, or adapt its
loader or binaries. The repository can be treated only as a pointer to an
external user-managed compatibility experiment, not as evidence that this
project supports DLSS on AMD or DLSS Neural Rendering. Any future AMD support
must use an independently integrable SDK or implementation with terms that
permit the intended use, and must remain separate from the native FSR adapter.

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
8. Integrate DLSS 5 neural rendering as a separate final render stage once
   NVIDIA's authorized Streamline feature interface, resource contract,
   supported platforms, and distribution terms for custom engines are available.
   Validate its art-direction controls and temporal behavior without changing
   the public renderer contract or removing fallbacks.

## Acceptance gates

- DLSS reports the actual loaded version and cannot be mislabeled.
- Original mode remains reference-compatible and does not silently enable DLSS.
- UI and subtitles are composed at output resolution after upscaling.
- Camera cuts, scene loads, resolution changes, and pause/resume invalidate history.
- Missing libraries or unsupported drivers fall back without a crash or black frame.
- DLSS and portable rendering produce identical authoritative simulation hashes.
- Packaged binaries pass the retail-content audit, license audit, and Gitleaks.
- Image-quality captures and frametime traces cover every supported quality preset.
