# FSR integration plan

AMD FidelityFX Super Resolution (FSR) is the planned optional AMD
temporal super-resolution backend for Modern+. It is never required to run the
game. The F10 graphics menu stores an FSR request, but the current renderer
reports the capability as unavailable and deterministically uses the available
spatial/native path. It does not expose a temporal fallback.

AMD FSR 1 is a separate spatial technique, specified in
[FSR1_SPATIAL_SPEC.md](FSR1_SPATIAL_SPEC.md). It uses only the current color
image and must not be presented as FSR 3 or as a temporal upscaler. The current
settings model keeps `FSR 1 (spatial)` distinct from temporal `FSR`. The SDL
renderer now has project-owned wrappers for AMD EASU and RCAS, generated DXIL,
SPIR-V, and MSL variants, and a two-pass GPU submission path. Runtime capability
is enabled only when shader, pipeline, sampler, and target creation succeed for
the current SDR UNORM swapchain format. sRGB and HDR formats still fall back
to native scaling. A bounded six-frame run submitted both passes on an Apple M4
GPU through macOS Metal; a second run used Linux Vulkan with Mesa llvmpipe
software rendering. Windows/D3D12, Steam Deck, broader Mac and Linux GPU
coverage, and image quality remain unverified.

## Boundary

FSR is not a name for generic scaling, a shader approximation, or the portable
temporal path. It becomes active only when the renderer has loaded a verified
official AMD implementation and can satisfy its input, API, platform, driver,
and redistribution requirements. The active overlay must identify the loaded
runtime and version exactly.

The future adapter must consume the renderer's vendor-neutral frame inputs:
color, depth, motion vectors, exposure, jitter, reactive mask, and HUD-less
color. UI remains composed at output resolution. It must not affect simulation,
input, saves, replay hashes, or mission state.

## Delivery gates

1. Verify the official AMD SDK, supported graphics APIs, redistribution terms,
   and platform support for each target.
2. Add a native adapter that owns no retail data and is optional at build and
   runtime.
3. Publish capability checks for API, driver, adapter, and loaded runtime.
4. Validate image quality, dynamic-resolution transitions, HDR, UI separation,
   frame pacing, and safe fallback behavior on AMD and non-AMD hardware.
5. Report the actual runtime version; never label a fallback path as FSR.

Until these gates are complete, temporal FSR intent is safely retained and
falls back to the native path. FSR 1 spatial shader variants are included for
SDL's DXIL, SPIR-V, and MSL backends, but runtime execution must be verified on
Windows, Linux, Steam Deck, and macOS. A portable temporal path is a separate
future deliverable, not a current fallback.

## Optional SDK build

The pinned FidelityFX SDK 1.1.4 host and Vulkan backend can be built without
enabling FSR in the game. Configure with
`OFF_BUILD_FSR3_SHADER_HEADERS=ON`, `OFF_BUILD_FSR3_VULKAN_HOST=ON`, and
`OFF_FIDELITYFX_SDK_ROOT` pointing to the SDK's `sdk/` directory, then build
`off_fsr3upscaler_host` and `off_fsr3upscaler_vk`. Shader permutations are
generated in the build directory; no SDK shader blobs or binaries are checked
into the repository.

The pinned SDK's public opaque context is smaller than its private FSR3 context.
The build creates a private header overlay with a larger opaque allocation and
uses the SDK's own compile-time size assertion before producing the libraries.
It also supplies the Microsoft CRT helpers assumed by the SDK source on
non-Windows compilers. Neither workaround modifies the SDK checkout. This is
build support only: it does not create an FSR runtime adapter, validated frame
inputs, or an enabled Modern capability.

This optional host and Vulkan library build completed on TRV2 with GCC 15,
Vulkan 1.4, and FidelityFX SDK commit
`c6efa6bf7f2027b3ec94f28578bb5965eabb9e55` on 2026-10-26. Other platforms and
the end-to-end renderer path remain unverified.

## Current AMD SDK split

Checked against AMD's official SDK and technique pages on 2026-10-29. AMD FSR
SDK 2.4.0 lists FSR Upscaling 4.1.2, FSR 3.1.6, and FSR 2.3.4. AMD's FSR
Upscaling 4.1.2 page now lists Vulkan 1.2 support at feature parity with its
DirectX 12 version. The same technique page lists Windows 10 and Windows 11 as
supported operating systems; it does not establish Linux, Steam Deck, or macOS
support. Therefore this newer FSR 4.1.2 path is a Windows candidate only until
AMD publishes and validates additional platform support. The published hardware
list includes RX 9000, RX 7000, and RX 6000 discrete GPUs, plus selected Ryzen
AI systems.

The project currently pins the older FidelityFX SDK 1.1.4 FSR 3.1.4 Vulkan
host. AMD's FSR 3 documentation lists both DirectX 12 and Vulkan, which remains
the candidate portable Vulkan route for Windows, Linux, and Steam Deck when the
renderer and runtime support are implemented. This older path is not FSR 4.1.2
and must be reported by its actual technique/version. Neither Vulkan path
provides a macOS Metal adapter here; macOS remains on the native/spatial
renderer until a supported backend exists.

References: [AMD FSR SDK 2.4.0](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK),
[AMD FSR Upscaling 4.1.2 platform and API support](https://gpuopen.com/amd-fsr-upscaling/),
[AMD FSR 3 API support](https://gpuopen.com/fidelityfx-super-resolution-3/),
[FidelityFX SDK 1.1.4](https://gpuopen.com/amd-fidelityfx-sdk-1/),
[AMD Vulkan backend](https://gpuopen.com/manuals/fidelityfx_sdk/reference_documentation/sdk/backends/vulkan_backend/v_k_backend/).

The SDL Vulkan external-pass API is a separate interop seam. The current SDL
frame loop does not call it, and the FSR host/Vulkan libraries are not linked
to a renderer adapter. The SDL pass can provide Vulkan handles and scoped
texture access, but this alone does not create an FSR context, collect the
SDK's device extensions, bind runtime capabilities, or dispatch a resolve.
Normal scene rendering also does not yet produce the depth and motion-vector
inputs required by FSR. Keep FSR unavailable until those inputs and the full
create/dispatch/destroy path are connected to a real scene frame.

## Primary references

- [AMD FidelityFX SDK manual](https://gpuopen.com/manuals/fidelityfx_sdk/)
- [AMD FidelityFX SDK API reference](https://gpuopen.com/manuals/fidelityfx_sdk/reference_documentation/sdk/ffx_s_d_k/)
