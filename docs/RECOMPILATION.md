# Recompilation research

Static recompilation translates machine instructions ahead of time into code
that can be built for a host platform. It still needs a runtime for the original
program's operating-system, graphics, audio, and input interfaces. It is not
the same as recovering independently authored source code.

## Candidate tools

- [pcrecomp](https://github.com/sp00nznet/pcrecomp) includes a 32-bit Windows
  PE analysis and x86-to-C pipeline, Ghidra tooling, and differential tests.
  Its main documentation describes a 32-bit Linux build; the
  [runtime design](https://github.com/sp00nznet/pcrecomp/blob/main/runtime/win32hle/README.md)
  also discusses 64-bit hosts, including Macs. Guest SEH/C++ exceptions and
  DirectX beyond DirectDraw 2 and DirectSound 1 are listed as unsupported.
  Neither Mac execution nor Freedom Fighters compatibility has been verified.
- [SR](https://github.com/M-HT/SR) statically recompiles a small set of DOS
  and Windows games for multiple targets. Its supported-game list does not
  establish compatibility with Freedom Fighters.
- [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) targets original Xbox
  XBE files and Xbox interfaces, not the Windows PE executable. Its x86
  translation and runtime design may still be useful research references.
- [DXVK](https://github.com/doitsujin/dxvk) implements Direct3D 8 through
  Vulkan for Linux/Wine. It is a graphics compatibility component, not a CPU
  recompiler or a verified adapter for this project's SDL renderer.

Recompilation by itself does not provide the scene producer, a working intro,
or temporal inputs needed by DLSS or FSR. Indirect calls, exceptions, original
API boundaries, and graphics integration would need separate validation on
each intended platform.

## Project boundary

Experiments can use original executables only in private, authorized local
workspaces. Generated output from a retail executable is retail-derived: do
not commit it, its generated metadata, the executable, or its assets. Public
work must follow [CLEAN_ROOM.md](../CLEAN_ROOM.md); an independently written
implementation requires a reviewed behavior-only specification. A synthetic
program can be used to assess a tool without publishing game-derived output.
No recompilation tool has yet been validated as a working route for this game.

## Synthetic feasibility check

On macOS arm64, pcrecomp revision `a85ea970f9e0640ab6546d2ca7a01fccb132973d`
ran its synthetic x86-32 differential suite against Unicorn: 371 of 372 tests
matched, with a `sqrtps` mismatch and a nonzero exit status. The mismatch was
reproduced for a negative operand: host `sqrtf` and the Unicorn reference
returned quiet NaNs with different sign bits. This is a bitwise difference in
the synthetic test; its effect on the game is unknown. A filtered shift test
passed. Separately, an original synthetic PE32 program with no imports
was analyzed and lifted to C. Its generated entry function compiled and ran
on macOS arm64 in a small harness with synthetic registers and guest memory,
returning the expected result. This did not test game code, a PE loader,
Win32 imports, Direct3D, or an end-to-end host runtime; the unmodified
revision's full differential suite did not pass.

A local pcrecomp experiment subsequently added default-mode `sqrtps` and
`sqrtss` result handling and SSE2 `paddw`, `punpcklwd`, and `punpckldq`
lifting. Its synthetic differential suite passed 394 of 394 cases on macOS
arm64. The unpack tests do not validate alignment or exception behavior. The
square-root change does not
model MXCSR exception flags, DAZ/FTZ, or guest rounding modes. These changes
remain local to the external tool checkout and are not an upstream release.
A later local fix prevents the Win32 host from retaining a successful boot
state after strict import binding fails. A synthetic regression test passed on
macOS arm64 and Linux; missing imports still need actual implementations.
The local host also has limited wide-character startup and module queries:
startup data is synthetic, module names are restricted to ASCII in UTF-16,
and pseudo-system modules do not support lookup by full path. Invalid guest
pointer exceptions are not modeled. These are not full Win32 implementations.
Local experiments also clamp lexical parent traversal at mapped drive and
overlay roots and provide a `statvfs`-backed disk-space query. This is not a
symlink sandbox, and Windows quota, network-path, access-control, and optical
media behavior is not fully modeled.
The local Win32 HLE experiment also implements COM task-memory allocation,
reallocation, and freeing using its guest heap. Synthetic tests cover zero-size
operations and allocation failures; external COM allocators and invalid-pointer
exceptions are not modeled.
An additional local `CreateEventW` implementation supports unnamed events
with default security only. Named events and explicit security attributes are
rejected; the existing wait-all and cross-process models remain incomplete.
Local HLE changes also add limited window-class metadata queries and
unregistration. The model does not implement complete module-local or global
class scope, system-class metadata, or unlimited reuse of class slots.
The local based-memory runtime now tracks logical guest virtual-memory regions
under a shared lock for checked copies, allocation, decommit, and release. Its
synthetic HLE suite passed 15 of 15 tests on Linux. This remains a simplified
model: Windows per-page commit/protection state and safe concurrent use of raw
guest pointers are not modeled, and checked copies are unavailable in flat mode.
A separate synthetic lifted guest also reached a host COM provider via an
indirect vtable call. A further synthetic test exercised public D3D8 COM slots
and x86 calling conventions for CreateDevice, Clear, and Present with stub
providers. It did not create a graphics device or test the game's renderer.

An additional macOS Metal experiment connected synthetic lifted guest
`Clear` and `Present` calls to a stub provider around an offscreen SDL GPU
render pass. The host drew a synthetic image using `SdlIntroRenderer` and
verified the resulting pixels by readback. The host supplied the active GPU
resources and inserted the draw itself: the experiment did not test swapchain
presentation, D3D8 rendering semantics, real guest draw calls, or the game.
Another synthetic experiment dispatched `DrawPrimitiveUP` from lifted x86
through a limited provider and drew a triangle with `SdlIntroRenderer`. Metal
offscreen readback matched the expected pixels, and four invalid input cases
were rejected. This covers one deliberately limited vertex format and does not
establish general D3D8 behavior or a production frame-lifetime integration.
The prototype now derives that format's offsets from an explicit synthetic
FVF and checks the selected fill and cull states. Other formats, shader handles,
and render states are not supported by the experiment.

The local pcrecomp checkout has an opt-in D3D8 HLE root and a limited
SDL-backed device, tested through synthetic PE import binding and COM calls.
The device handles a restricted create, clear, and present path; drawing and
many normal D3D8 methods remain unsupported. It does not render the game.
It requires an experimental safe-file-view
policy that rejects shared file-backed mappings and uses private snapshots for
copy-on-write views; this is not full Windows mapping semantics. Synthetic
default and opt-in HLE suites each passed 23 of 23 tests on Linux. Deferred
device cleanup requires the owner thread's pump to continue running.

The local checkout also provides an experimental screen-owned SDL2 rendering
path. Its synthetic software-renderer test checks draw, ownership, failure
recovery, and GDI fallback; the corresponding HLE suite passed 21 of 21 tests
on Linux. The limited local D3D8 device uses its offscreen buffer, but it is
not connected to the game. Screen opening
requires serialized initialization and renderer-owner thread use; general
shutdown and full guest framebuffer concurrency are not implemented.
The local checkout also has a screen-owned offscreen-buffer experiment. Its
synthetic SDL2 software-renderer tests cover clear, triangle rendering, resize,
repeated presentation, owner-thread checks, and selected failure recovery.
This does not establish D3D8 swap semantics or physical display behavior.

A synthetic 16×16 triangle comparison on Linux found pixel differences
between SDL2's software renderer and WineD3D with llvmpipe. Trying ±0.5-pixel
coordinate offsets did not consistently match the reference across the tested
edges, culling, interpolation, and blending cases. WineD3D is not a native
Windows or hardware oracle; the local device still rejects drawing until its
raster semantics are verified.

A separate experimental CPU raster path has been exercised through synthetic
PE import binding and D3D8 device COM calls. Its complete 16×16 RGBA buffers
matched WineD3D/llvmpipe on Linux for 17 opaque, constant-color triangle cases.
The cases cover selected edges, clipping, subpixel positions, and culling.
They do not establish behavior for every accepted coordinate, blending,
interpolation, double coverage, physical presentation, or native Windows.
The path remains a local opt-in experiment; the default device rejects draws
and the game is not rendered by it. On Linux, the complete synthetic HLE
self-test suite passed both with the triangle experiment disabled and with it
enabled (23 tests in each configuration).

The local D3D8 root can also report the current SDL window's display mode when
the display is available and its format matches the supported X8R8G8B8 profile.
It does not advertise fullscreen modes. Synthetic Linux tests cover headless
and unsupported cases, checked guest writes, display lookup failure, and
concurrent snapshot access. macOS and Windows have not been verified for this
change.

The local pcrecomp checkout also has a separate opt-in fixed-function 3D
experiment for a limited `XYZ|DIFFUSE` triangle profile. It includes transforms,
viewport clipping, interpolated color, and D16 depth. Synthetic Linux tests
exercise its COM path and full color/depth rollback on injected failures.
Comparisons with WineD3D/llvmpipe still show differences at some clipped edges
and one-step color differences with varying W. This is not general D3D8 parity
and has not been verified against native Windows, macOS, or the game.

An additional opt-in texture experiment supports one managed A8R8G8B8 level,
point sampling, clamp addressing, and a limited `XYZ|TEX1` triangle profile.
It rejects textured triangles with varying clip W. Five synthetic constant-W
images matched stored WineD3D/llvmpipe references byte for byte; this does not
establish general texture or hardware parity. The guest-memory and device
self-tests also passed with the texture experiment enabled, with 3D enabled
without textures, and with D3D8 disabled on Linux.
Separate varying-W interpolation experiments were reproducibly rebuilt and
compared with the saved Wine reference; none consistently removed the observed
differences, so the varying-W restriction remains.

The texture-enabled experiment also has bounded managed vertex and 16-bit
index buffers for triangle-list submission. Synthetic COM tests compare the
buffered path with `DrawPrimitiveUP` and exercise lock staging, bounds and
failure handling. Concurrent guest writes during `Unlock` are unsupported;
the buffered path has not been verified against native D3D8 or the game.

The local pcrecomp checkout also fixes a D3D8 build failure when the optional
software 3D profile is disabled. On Linux, the D3D8 device self-test passed
with that profile disabled, with 3D enabled, and with textured 3D enabled.
This build fix does not establish renderer or game parity.

Seventeen additional synthetic shared-edge quad cases matched WineD3D/llvmpipe
in complete RGBA buffers, including selected subpixel phases, winding orders,
diagonals, and viewport offsets. A separate single slanted triangle still
shows edge-coverage differences; these cases do not establish general raster
or native Windows parity.

A further independent set of 22 synthetic cases produced 20 exact full-RGBA
matches against WineD3D/llvmpipe. One thin triangle differed at a coverage
pixel; one clipped quad differed at a shared edge where both rendered a pixel
but selected different triangle colors. A subpixel coverage hypothesis fitted
to earlier cases missed another pixel in this set, so it is not used by the
implementation. These comparisons do not establish native hardware behavior.
An expanded set of 36 synthetic cases produced 31 exact full-RGBA matches;
four clipped-quad variants differed at one shared-edge color pixel and one
thin triangle differed at one coverage pixel. The fitted subpixel hypothesis
also missed a coverage pixel, so it remains outside the implementation.
Eleven later synthetic bottom-clipping cases further contradicted that specific
post-clipping quantization model: only four coverage masks matched WineD3D.
This does not rule out other precision models or establish hardware behavior.
Thirteen subsequent cases isolated a single boundary sample: eight differed
at that pixel, while the tested model matched four coverage masks. These
results do not identify a general clipping or edge-ownership rule.
