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
- Packed `ZGF` and `GMS` envelopes are decoded from raw DEFLATE or stored representations with exact size and Adler-32 validation.
- Decoded `ZGF` bundles expose named embedded payloads through owned, bounded views.
- Decoded `GMS` images expose bounds-checked object-source transforms, attachments, pool assignments, cross-resource `BUF` offsets, and identifier directories without exposing retail strings.
- Packed RMC/RMI geometry references decode as tagged, 112-byte-aligned runtime object-pool handles; they are not raw GMS offsets.
- Scene `SUP` resources are parsed as bounded `DLCF` dependency lists in both observed scalar and array layouts.
- Scene `TEX` resources are parsed into image, mip, palette, and sequence models with bidirectional index validation, then decoded through portable CPU paths to RGBA8.
- Scene `PRM` resources are parsed into indexed primitive descriptors, portable position/normal/color/texture-coordinate vertices, and bounds-checked topology batches. Optional auxiliary streams remain opaque.
- Scene `RMC` and `RMI` resources are parsed into fully traversed and world-queryable quantized octrees, bounds, portable object transform/extent descriptors, and validated packed geometry references.

These rules protect the native runtime from malformed user-supplied data. They do not yet define the complete game-data mount lifecycle or any resource-family schema.

## Verification

Synthetic fixtures cover stored and deflated payloads, the Glacier footer, case and slash normalization, traversal rejection, CRC corruption, directory-to-archive overlay precedence, mount removal, bounded streaming reads, and end-of-file rejection. The native installation verifier opens the supported retail global stream bank through a streaming view. It also opens `StartLoader.ZIP`, reads its `ZGF` member through the archive parser, verifies its CRC, and checks the corpus-proven file-size invariant.

Private executable evidence reports that an older archive entry is invalidated when the same virtual file appears in a newer archive. The public VFS expresses only that interoperability behavior; it does not reproduce the original implementation.

No retail payload or filename inventory is embedded in the test fixtures.

The installation verifier parses the single `SUP` member in each of all 90 scene archives, including decompression and CRC validation, and checks that the corpus shape matches the supported build. The format is documented in [SCENE_SUPPORT_FORMAT.md](SCENE_SUPPORT_FORMAT.md).

It also decodes the single `ZGF` and `GMS` member in each archive. The resulting 180 payloads total 34,221,064 and 33,436,872 bytes respectively; all packed sizes, output sizes, and checksums are validated. The 90 decoded `ZGF` bundles contain 1,019 named entries and 34,161,792 embedded bytes. The GMS parser validates 179,838 object-source directory entries, their finite bases and positions, NUL-terminated BUF object names, 34,218 attachment tables with 39,885 entries, 5,765 auxiliary BUF extents, exact assignment across 29,450 pool groups and 24 class counters per group, and 154,941 NUL-terminated identifier references without exposing retail text. Its diagnostic registry maps all 71 corpus source types into the executable's 102 exported geometry classes, while 115,977 class-qualified source uses resolve exactly to packed indexes in their archive's PRM catalog. The two empty GMS images are the only archives without BUF. Of 3,002 present RMC/RMI runtime handles, 2,998 map back to a local GMS source and four slot-zero handles belong to those empty images. See [PACKED_RESOURCE_FORMAT.md](PACKED_RESOURCE_FORMAT.md), [ZGF_FORMAT.md](ZGF_FORMAT.md), and [GMS_FORMAT.md](GMS_FORMAT.md).

The same full-corpus pass parses every `TEX` member and validates 23,522 image records and 19 sequence records. It decodes a retail reference from each of the four observed encoding families and discards the output immediately. Encoded and decoded mip bytes are held only in transient memory. See [TEXTURE_FORMAT.md](TEXTURE_FORMAT.md).

Every `PRM` member is also parsed during installation verification. The supported corpus contains 61,451 entries, including 27 flagged references, plus 40,071 primitive-to-texture links, 2,820,961 decoded primary vertices, 461,344 topology batches, and 4,412,738 individually range-checked vertex indexes. Every nonzero primitive texture ID is resolved against the paired `TEX` catalog. See [PRIMITIVE_FORMAT.md](PRIMITIVE_FORMAT.md).

`RenderAssetBindings` converts the parsed pair into a renderer-facing table. Each
ordinary PRM entry maps to its primitive index, optional TEX image index, preserved
selector flag, minimum and maximum vertex alpha, and a portable alpha class. Special
PRM references are excluded until their indirection semantics are known.
Construction rejects missing or duplicate texture IDs and empty vertex tables, so
the render backend never performs unchecked cross-file ID lookup. The full-corpus
audit records 46,140 opaque, 12,751 variable-alpha, and 2,533 fully transparent
ordinary primitives. All variable-alpha primitives carry selector bit 11, while
5,980 all-zero or all-255 primitives carry it too; therefore the public API keeps
the observed flag separate from inferred render-state policy.
Each binding also exposes the confirmed triangle-strip or line-list topology, a
contiguous portable index buffer, and draw ranges that preserve every source batch.
The full installation yields 57,284 triangle-strip and 4,140 line-list bindings;
their 461,344 ranges cover all 4,412,738 indexes exactly once.

The runtime preview loader applies that contract to `FF-StartUp.ZIP`. It chooses
the first supported textured triangle strip, copies its vertices, flattened index
buffer, and draw ranges into an owned render asset, decodes mip zero to RGBA8, and
computes its model-space bounds. It rejects missing resources, missing texture
bindings, empty mip chains, and indexed meshes without a nondegenerate triangle
before SDL GPU is initialized. Bounds include only referenced vertices so unused
storage cannot distort the diagnostic projection. A scene-resolution path follows
an RMC primary geometry handle to its exact local GMS slot and then its PRM
reference, preserving map-entry, descriptor, handle, source, and transform
identity. Missing local sources and non-primitive sources are not guessed. Because
the startup RMC source is not a primitive, the visible preview currently uses a
separate deterministic first-GMS-match fallback.
The resolver emits primary and present secondary references in stable map-entry
order. Each result is classified as a local primitive, no matching local source,
source without a primitive, missing PRM record, or unresolved flagged PRM alias,
and retains the available map, GMS, and PRM indexes. Secondary references are
additional objects and never substitute for an unresolved required primary.
Installation verification applies this manifest to both RMC and RMI in all 90
scenes. The supported corpus contains 3,002 results: 2,801 primary and 201
secondary; 220 resolve to direct local primitives, four have no matching local
source, and 2,778 resolve to local sources without direct primitives. No result
references a missing PRM record or an unresolved flagged PRM alias.
`SceneRenderAsset` then converts every direct local primitive result across an
ordered set of RMC/RMI layers into owned renderer resources. It deduplicates PRM
meshes and TEX images, never deduplicates instances, retains all resolution
outcomes, and keeps GMS and map transforms separate. The corpus-derived capacity
observations, defensive limits, and current rendering boundary are documented in
[SCENE_RENDER_ASSET.md](SCENE_RENDER_ASSET.md).
The platform layer applies `world = basis * local + position`, converts that owned
asset to a bounds-normalized diagnostic projection, uploads vertex and 16-bit index
buffers plus the RGBA8 texture, and submits every preserved range as an indexed
triangle-strip draw. A centralized pre-upload validator rejects incomplete assets,
non-contiguous or overflowing draw ranges, out-of-range indexes, inconsistent RGBA
dimensions, invalid bounds, and non-finite source/map transforms, vertex attributes,
or transformed positions before any SDL GPU indexing occurs. Synthetic tests
cover primary and secondary ordering, tagged slot zero, absent local sources,
non-primitive sources, missing and duplicate identities, unresolved high-bit PRM
aliases, malformed indexes, construction, and failure without embedding retail
content.

Every `RMC` and `RMI` member is parsed as well. The supported corpus contains 2,587 nodes and 1,612 entries in `RMC`, plus 1,359 nodes and 1,189 entries in `RMI`. Every octree node is reached exactly once, every spatial element has exactly one owner, and every fixed-size descriptor reference and quantized bound is range checked. World-space bounds queries reproduce the recovered quantization, loose-cell, and half-open intersection rules. The final descriptor words are now confirmed as one required and one optional packed geometry reference rather than floating-point scale values. See [RENDER_MAP_FORMAT.md](RENDER_MAP_FORMAT.md).

The audio format model is documented in [AUDIO_FORMAT.md](AUDIO_FORMAT.md).
