# Third-party dependencies

OpenFreedomFighters uses established portable libraries through their public APIs. No third-party source is currently vendored in this repository.

| Dependency | Purpose | Upstream license | Integration |
|---|---|---|---|
| [zlib](https://github.com/madler/zlib) | ZIP deflate and CRC-32 | zlib License | system development package; commit-pinned source build on Windows CI |
| [libogg](https://github.com/xiph/ogg) | Ogg container support | BSD-style license | system development package; commit-pinned source build on Windows CI |
| [libvorbis](https://github.com/xiph/vorbis) | Vorbis decode and synthetic test encoding | BSD-style license | system development package; commit-pinned source build on Windows CI |
| [Capstone](https://github.com/capstone-engine/capstone) | private code-boundary analysis | BSD 3-Clause | pinned optional Python analysis dependency |

These dependencies contain no Freedom Fighters code or data. Their licenses and notices remain governed by their respective upstream distributions and must be included when binary packaging begins.
