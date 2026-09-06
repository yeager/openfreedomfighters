# Continuous integration and releases

The workflow structure is self-contained in this repository and keeps platform
jobs, security audits, data-free artifacts, and release gates explicit so it can
be compared mechanically with the requested OpenCapture workflow convention.

## Build workflow

Every push to `main`, pull request targeting `main`, and manual dispatch runs:

- a tracked-file extension audit plus full-history and working-tree Gitleaks scans using a checksum-pinned CLI;
- CMake/Ninja builds with SDL3, zlib, and Xiph Vorbis dependencies, followed by CTest on Ubuntu 24.04 and macOS 14;
- a CMake/MSVC build and CTest on Windows Server 2022 using source-built or checksum-pinned native dependencies;
- Python analysis-tool tests and ASan/UBSan tests on Linux.
- checksum-pinned DXC compilation and validation of the project-owned picture
  shader's generated HLSL, with a generated-header artifact and comparison when
  a packaged DXIL header is present.

All CI tests use synthetic fixtures. GitHub-hosted runners never receive a retail installation, extracted resources, private disassembly, or decoded game audio.

CMake accepts a compatible system SDL3 package and otherwise downloads the
official 3.4.10 release archive with a required SHA-256 checksum. SDL is linked
statically for fallback builds. CI does not open a window because hosted runners
do not contain retail data or represent target GPU hardware; the instanced
retail-mesh draw smoke test is run locally and hardware validation remains an
explicit release gate. The Linux x86-64 release artifact is intended to become the
Steam Deck build, but validation on Deck hardware remains pending.
The repository Gitleaks configuration excludes only ignored CMake build products;
tracked history and all source, documentation, configuration, and other untracked
working files remain in scope.

## Release workflow

Tags matching `v*` and explicit manual dispatches build and test data-free binaries for Linux, macOS, and Windows. The requested release version must exactly match the version in `CMakeLists.txt`. Before GitHub Release creation, the workflow verifies that all expected artifacts exist, audits their filenames for prohibited game-data formats, and runs Gitleaks.

Release binaries contain only independently authored code and permitted runtime dependencies. They still require a legally purchased supported installation at runtime. Until the roadmap reaches a playable gate, tagged packages are development snapshots rather than playable releases.
