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
matched, with a `sqrtps` mismatch and a nonzero exit status. A filtered shift
test passed. Separately, an original synthetic PE32 program with no imports
was analyzed and lifted to C. Its generated entry function compiled and ran
on macOS arm64 in a small harness with synthetic registers and guest memory,
returning the expected result. This did not test game code, a PE loader,
Win32 imports, Direct3D, or an end-to-end host runtime; the full differential
suite cannot be considered passing.
