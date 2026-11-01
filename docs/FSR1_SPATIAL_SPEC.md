# FSR 1 spatial presentation specification

## Purpose

Add an optional, cross-vendor spatial upscaling path for Modern rendering. It
is a distinct feature from the existing FSR request, which is reserved for the
planned temporal FSR adapter. The menu and runtime status must call the spatial
technique **FSR 1** so users can distinguish it from temporal FSR.

AMD describes FSR 1 as a spatial upscaler that uses the current anti-aliased
frame without frame history or motion vectors. Its image path has two stages:
EASU reconstructs the output-resolution image, then RCAS sharpens that image.
AMD recommends running the stages after tone mapping and before high-frequency
overlays such as subtitles. See the [official FSR 1 overview](https://gpuopen.com/fidelityfx-superresolution/)
and the [official FSR 1 source repository](https://github.com/GPUOpen-Effects/FidelityFX-FSR).

The F10 graphics menu exposes FSR 1 sharpness from 0% to 100% in 5% steps.
The value maps to the FSR 1 dispatch interval [0, 1]; 0% disables additional
sharpening and 100% selects its maximum. The default is 25%, preserving the
previous RCAS attenuation. This setting affects FSR 1 only and is retained
independently of the selected upscaler so it can be adjusted before enabling
FSR 1.

## Frame contract

- Input is the current scene color image at the effective render extent, after
  tone mapping and scene anti-aliasing when available.
- EASU writes a full output-resolution image. RCAS consumes that image and
  writes the final spatial result.
- The graphics menu, subtitles, and other UI are composed at output resolution
  after RCAS. The spatial pass must not rescale UI.
- The path uses no depth, motion vectors, exposure history, temporal history,
  jitter, or frame-generation input. It must not change simulation, input,
  saves, or replay state.
- Render-size and output-size changes recreate or resize the pass resources and
  refresh the spatial constants before the next dispatch.
- If the selected shader format, texture formats, or required GPU resources are
  unavailable, retain the user's FSR 1 request and report the existing
  deterministic spatial/native fallback. Do not label that fallback FSR 1.
- At render scale equal to or above output resolution, the spatial adapter may
  be reported as available but is not an active super-resolution pass. The
  renderer must not claim an upscale occurred for that frame.

## Runtime and portability

The renderer must expose FSR 1 only after it can create both passes, bind their
input/output textures, submit them in order, and place the UI after their
resolve. The source currently uses SDL GPU on its platform-selected backend;
generated shader variants must match SDL's supported DXIL, SPIR-V, and MSL
formats. Each backend remains unavailable until that variant and its texture
requirements have been validated there.

FSR 1 is spatial and may be used on non-AMD GPUs. It must not depend on a
vendor-specific runtime or the SDL Vulkan external-pass bridge. Temporal FSR,
DLSS, and XeSS remain separate adapters with their own frame-input and runtime
requirements.

## Source and licensing boundary

The upstream FSR 1 source is MIT-licensed. Do not copy its SDK source tree or
generated shader binaries into the repository. If implementation consumes the
upstream source at build time, pin the exact upstream revision, keep the
checkout outside the source tree, and retain the upstream license with that
dependency. Generated project output must derive only from the project's own
wrapper/source files and the pinned public dependency.

## Acceptance evidence

The Modern menu and settings resolver keep FSR 1 independent of temporal FSR.
A bounded six-frame run on an Apple M4 GPU through macOS Metal and a six-frame
Linux Vulkan run submitted EASU then RCAS before output-size UI. The Linux run
used Mesa llvmpipe software rendering, so it verifies the Linux/Vulkan path but
not physical GPU performance. DXIL and SPIR-V shader validation passed.
D3D12/Windows, Steam Deck hardware, Linux hardware, image quality, and broader
Mac GPU support still require validation. Unsupported formats use native
scaling and are not reported as active FSR 1.
