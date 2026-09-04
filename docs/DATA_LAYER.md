# Portable data layer

The first Phase 1 component is a read-only ZIP archive reader and overlay virtual file system. It is intentionally narrower than a general ZIP implementation and accepts only structures required by the supported retail data.

## Current contracts

- All integer and byte-range access is bounds checked.
- Standard single-disk ZIP archives with stored (method 0) or raw-deflate (method 8) members are supported.
- ZIP64, encrypted members, split archives, unsupported compression methods, and inconsistent directories are rejected.
- The engine-specific footer observed after 89 of the 90 retail ZIP end records is accepted up to a strict 4 KiB trailing-data limit.
- Absolute paths, drive-qualified paths, NUL bytes, empty path segments, and `.` or `..` segments are rejected.
- Paths are normalized to forward slashes and compared case-insensitively. Duplicate normalized member names within one archive are rejected.
- Local and central headers must agree on flags, compression method, and normalized filename.
- Every extracted payload is checked against its declared size and CRC-32.
- A single member is limited to 128 MiB and the declared uncompressed total of an archive is limited to 1 GiB.
- Archives form an overlay: the most recently mounted archive wins when multiple archives provide the same normalized virtual path.

These rules protect the native runtime from malformed user-supplied data. They do not yet define the complete game-data mount lifecycle or any resource-family schema.

## Verification

Synthetic fixtures cover stored and deflated payloads, the Glacier footer, case and slash normalization, traversal rejection, CRC corruption, and overlay precedence. The native installation verifier also opens the supported retail `StartLoader.ZIP`, reads its `ZGF` member through this parser, verifies its CRC, and checks the corpus-proven file-size invariant.

No retail payload or filename inventory is embedded in the test fixtures.
