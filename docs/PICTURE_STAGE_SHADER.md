# Picture texture-stage shader

The project-owned fragment shader evaluates one resolved texture stage. It takes
interpolated diffuse color, UVs, one texture/sampler and a 48-byte uniform block.
RGB and alpha independently support argument-one selection, doubled modulation
and saturated addition. Arguments are texture, diffuse, stage-zero current
(equivalent to diffuse), and an explicitly supplied ARGB texture factor.
These operations follow Microsoft's public
[texture-operation](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dtextureop)
and [argument](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dta)
contracts; the game's choice of operations comes from the separately recovered
material/request paths.

Disabling RGB terminates texture processing and returns diffuse. Disabled alpha
with enabled RGB is rejected: Microsoft defines that combination as undefined.
The recovered special-material request can produce it, so this shader does not
silently assign that branch an invented alpha equation. Its eventual supported
behavior needs separate evidence or an explicitly documented compatibility policy.

`pack_picture_stage_uniforms` consumes resolved stage fields, not optional
requests. The caller applies binding requests before material requests and
retains every omitted field. Texture identity is managed separately: the shader
requires a real bound texture and sampler and does not choose a fallback asset.
Fog, alpha testing, later texture stages, blending, depth, culling, filtering and
projection are outside this fragment operation. No default for them is implied.

## Formats and regeneration

The checked-in header contains SPIR-V and generated Metal source. Normal builds
do not need shader compiler executables. CMake checks the GLSL source hash and
requires regeneration after source changes. The generator validates SPIR-V,
reflects the input/output, descriptor and uniform layout, and checks Metal slots.
It also emits HLSL from that same SPIR-V. The separate DXIL build step uses DXC
shader model 6.0, retains reflected register spaces and produces a C++ header:

```sh
python3 tools/build_picture_dxil.py --dxc /path/to/dxc \
  --work-dir /path/to/existing/private-build-directory
```

The `picture-dxil` CI job uses Microsoft's checksum-pinned DXC 1.9.2602 Linux
x86-64 distribution and uploads only the generated header. Compilation alone
does not verify D3D12 pixels or establish original shader behavior.

```sh
python3 tools/build_picture_shader.py \
  --glslang /path/to/glslangValidator \
  --spirv-cross /path/to/spirv-cross \
  --spirv-val /path/to/spirv-val \
  --work-dir /path/to/existing/private-build-directory
```

Add `--check` to compare without modifying the generated header. Compiler outputs
use the explicit work directory, never a default temporary location. This header
was generated with glslang 16.2.0, SPIRV-Cross package
`2021.01.15+1.4.335.0-1` and SPIRV-Tools `2026.1-1`. Different compiler versions
may generate different bytes; review and regenerate intentionally.

The SDL factory selects SPIR-V or MSL and fails explicitly for other formats.
Vulkan execution is verified locally; Metal compilation/execution is not yet
verified. DXIL packaging and D3D12 verification remain open. Windows is still a
target platform; this component does not yet cover its D3D12 backend. The normal
runtime's existing shader selection is unchanged.

## Verification

All 64 local CTest executables pass without skips. Fourteen offscreen Vulkan
cases compare every RGBA channel against independently computed expectations,
with one UNORM byte of tolerance. Cases cover argument selection, RGB/alpha
saturation and independent operations. Four cases connect the actual material
resolver's explicit feature masks to inherited stage state, uniform packing and
GPU output. The feature-zero case preserves the operation while applying the
resolver's white texture-factor request; feature two preserves the colored factor.

CPU checks reject invalid enums, undefined alpha combinations and null devices.
Targeted GCC and ASan/UBSan runs also pass. Generated output reproduces exactly
with the listed tools. Test inputs are synthetic, not substituted game assets or
proof of original runtime masks, device rounding or full intro rendering.
