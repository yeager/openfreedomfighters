# FSR integration plan

AMD FidelityFX Super Resolution (FSR) is the planned optional AMD
super-resolution backend for Modern+. It is never required to run the game.
The F10 graphics menu stores an FSR request, but the current renderer reports
the capability as unavailable and deterministically falls back to portable
temporal upscaling or native rendering.

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

Until these gates are complete, FSR intent is safely retained and the portable
path stays available on Windows, Linux, Steam Deck, and macOS where supported.

## Primary references

- [AMD FidelityFX SDK manual](https://gpuopen.com/manuals/fidelityfx_sdk/)
- [AMD FidelityFX SDK API reference](https://gpuopen.com/manuals/fidelityfx_sdk/reference_documentation/sdk/ffx_s_d_k/)
