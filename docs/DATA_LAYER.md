# Portable data layer

The first Phase 1 component is a read-only ZIP archive reader and overlay virtual file system. It is intentionally narrower than a general ZIP implementation and accepts only structures required by the supported retail data.

## Current contracts

- All integer and byte-range access is bounds checked.
- Standard single-disk ZIP archives with stored (method 0) or raw-deflate (method 8) members are supported.
- ZIP64, encrypted members, split archives, unsupported compression methods, and inconsistent directories are rejected.
- The engine-specific footer observed after 89 of the 90 retail ZIP end records is accepted up to a strict 4 KiB trailing-data limit.
- Absolute paths, drive-qualified paths, NUL bytes, empty path segments, and `.` or `..` segments are rejected.
- Safe, empty ZIP directory records are validated and omitted from the file index; directory records carrying payload bytes are rejected.
- Paths are normalized to forward slashes and compared case-insensitively. Duplicate normalized member names within one archive are rejected.
- Local and central headers must agree on flags, compression method, and normalized filename.
- Every extracted payload is checked against its declared size and CRC-32.
- A single member is limited to 128 MiB and the declared uncompressed total of an archive is limited to 1 GiB.
- Read-only directory and archive mounts form one overlay: the most recently mounted source wins when multiple sources provide the same normalized virtual path.
- Every mount receives a stable identifier and can be removed explicitly, allowing scene resources to be retired without disturbing lower-priority global mounts.
- Directory mounts reject symbolic links, normalized-name collisions, files larger than 256 MiB, and declared totals larger than 1 GiB.
- Loose files expose bounded random-access views, allowing global audio banks and other large sources to be consumed in small chunks without whole-file allocation.
- A random-access view detects replacement, resizing, or conversion of its source into a symbolic link before each read.
- Scene `SUP` resources are parsed as bounded `DLCF` dependency lists in both observed scalar and array layouts.
- Scene `TEX` resources are parsed into image, mip, palette, and sequence models with bidirectional index validation, then decoded through portable CPU paths to RGBA8.
- Scene `PRM` resources are parsed into indexed primitive descriptors, portable position/normal/color/texture-coordinate vertices, and bounds-checked topology batches. Optional auxiliary streams remain opaque.
- Scene `RMC` and `RMI` resources are parsed into spatial headers, preserved packed hierarchies, quantized bounds, and portable object transform/extent descriptors.

These rules protect the native runtime from malformed user-supplied data. They do not yet define the complete game-data mount lifecycle or any resource-family schema.

## Verification

Synthetic fixtures cover stored and deflated payloads, the Glacier footer, case and slash normalization, traversal rejection, CRC corruption, directory-to-archive overlay precedence, mount removal, bounded streaming reads, and end-of-file rejection. The native installation verifier opens the supported retail global stream bank through a streaming view. It also opens `StartLoader.ZIP`, reads its `ZGF` member through the archive parser, verifies its CRC, and checks the corpus-proven file-size invariant.

Private executable evidence reports that an older archive entry is invalidated when the same virtual file appears in a newer archive. The public VFS expresses only that interoperability behavior; it does not reproduce the original implementation.

No retail payload or filename inventory is embedded in the test fixtures.

The installation verifier parses the single `SUP` member in each of all 90 scene archives, including decompression and CRC validation, and checks that the corpus shape matches the supported build. The format is documented in [SCENE_SUPPORT_FORMAT.md](SCENE_SUPPORT_FORMAT.md).

The same full-corpus pass parses every `TEX` member and validates 23,522 image records and 19 sequence records. It decodes a retail reference from each of the four observed encoding families and discards the output immediately. Encoded and decoded mip bytes are held only in transient memory. See [TEXTURE_FORMAT.md](TEXTURE_FORMAT.md).

Every `PRM` member is also parsed during installation verification. The supported corpus contains 61,451 entries, including 27 flagged references, plus 2,820,961 decoded primary vertices, 461,344 topology batches, and 4,412,738 individually range-checked vertex indexes. See [PRIMITIVE_FORMAT.md](PRIMITIVE_FORMAT.md).

Every `RMC` and `RMI` member is parsed as well. The supported corpus contains 1,612 `RMC` entries and 1,189 `RMI` entries, each with a uniquely referenced fixed-size descriptor and range-checked quantized bounds. See [RENDER_MAP_FORMAT.md](RENDER_MAP_FORMAT.md).

The audio format model is documented in [AUDIO_FORMAT.md](AUDIO_FORMAT.md).
