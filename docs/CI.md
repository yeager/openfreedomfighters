# Continuous integration and releases

The workflow structure follows the sibling [OpenCaptive](https://github.com/yeager/OpenCaptive) project while adapting its commands and artifacts to OpenFreedomFighters.

## Build workflow

Every push to `main`, pull request targeting `main`, and manual dispatch runs:

- a tracked-file extension audit and a full-history Gitleaks scan;
- CMake/Ninja builds and CTest on Ubuntu 24.04 and macOS 14;
- a CMake/MSVC build and CTest on Windows Server 2022;
- Python analysis-tool tests and ASan/UBSan tests on Linux.

All CI tests use synthetic fixtures. GitHub-hosted runners never receive a retail installation, extracted resources, private disassembly, or decoded game audio.

## Release workflow

Tags matching `v*` and explicit manual dispatches build and test data-free binaries for Linux, macOS, and Windows. The requested release version must exactly match the version in `CMakeLists.txt`. Before GitHub Release creation, the workflow verifies that all expected artifacts exist, audits their filenames for prohibited game-data formats, and runs Gitleaks.

Release binaries contain only independently authored code and permitted runtime dependencies. They still require a legally purchased supported installation at runtime. Until the roadmap reaches a playable gate, tagged packages are development snapshots rather than playable releases.
