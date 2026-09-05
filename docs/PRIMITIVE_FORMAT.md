# Primitive-catalog format

Each of the 90 scene archives in the supported Steam build contains one `PRM` primitive catalog. The native parser validates 61,451 indexed entries, 461,344 topology batches, and 4,412,738 vertex indexes without exporting retail geometry.

## Envelope and index

All integer fields are little-endian. The 16-byte envelope contains:

| Offset | Type | Meaning |
|---:|---|---|
| 0 | `u32` | end of the shared base-data region |
| 4 | `u32` | start of the trailing primitive index |
| 8 | `u32` | repeated primitive-index offset |
| 12 | `u32` | primitive-index entry count |

The two index offsets must agree, and the index must occupy the exact remainder of the file with one 32-bit value per entry. The low 31 bits of each value are an even byte offset to a unique 124-byte primitive descriptor before the index. The high bit identifies a special reference form; 27 such entries occur in the corpus and are retained as flagged references rather than interpreted as ordinary geometry.

The base-data boundary is 76,476 in every observed catalog. All catalogs carry the same 49 initial index offsets. Main-scene catalogs may add one flagged reference into the base region before their scene-specific entries.

## Primitive descriptors

Executable control flow and cross-file relationships confirm these fields for an ordinary descriptor:

| Descriptor offset | Type | Portable meaning |
|---:|---|---|
| 0 | `u16` | format flags |
| 2 | `u16` | primitive kind, observed as 0 or 3 |
| 4 | `u16` | packed texture selector |
| 6 | `u16` | zero padding |
| 8 | `u32` | optional secondary-data offset |
| 14 | `u16` | vertex count |
| 16 | `u32` | auxiliary-data offset |
| 20 | `u32` | vertex-data offset |
| 60 | `u32` | topology-data offset |
| 64 | `u32` | topology size in 16-bit words |

All four data offsets are file-relative, even, and point backward from the descriptor. Optional secondary and auxiliary streams vary with the format flags and remain deliberately opaque until their roles are independently confirmed.

The texture selector uses bits 0–10 as a texture-image ID, with zero meaning no
texture, and bit 11 as an independently preserved selector flag. Bits 12–15 and
the following 16-bit word are zero throughout the supported corpus. Every one of
the 40,071 nonzero IDs resolves to an image in the paired `TEX` catalog; 18,731
selectors carry bit 11. The flag's rendering effect remains deliberately unnamed
until its runtime use is independently confirmed. It is primitive-local rather
than image metadata: within individual archives, 516 texture images are referenced
by both flagged and unflagged primitives.

## Vertex records

Every ordinary descriptor in the supported corpus uses the same 36-byte primary vertex record:

| Vertex offset | Type | Portable meaning |
|---:|---|---|
| 0 | `f32[3]` | position |
| 12 | `f32[3]` | normal |
| 24 | packed `u32` | vertex color |
| 28 | `f32[2]` | texture coordinates |

Vertex colors use the same packed channel masks confirmed for texture palettes and are converted to RGBA8. Floating-point components are preserved without normalization or coordinate-system conversion. Non-finite values are rejected. The complete corpus contains 2,820,961 decoded primary vertices; every 36-byte table fits before its owning descriptor.

The renderer-facing model classifies each ordinary primitive from its decoded alpha
values without assigning an unproven meaning to the selector flag. Of 61,424
ordinary primitives, 46,140 have alpha 255 at every vertex, 2,533 have alpha zero
at every vertex, and 12,751 have another or varying alpha range. Every primitive
in the third class carries selector bit 11; the converse is not true. This is
strong evidence that the flag permits an alpha-related path, but it does not yet
prove whether the original runtime selects blending, alpha testing, sorting, or a
combination of those states.

## Topology batches

The topology table is an exact sequence of little-endian 16-bit words. Its first word is the batch count. Each batch then contains an index count followed by that many vertex indexes:

```text
u16 batch_count
repeat batch_count times:
    u16 index_count
    u16 vertex_indexes[index_count]
```

Every batch consumes at least one index, every referenced index must be below the descriptor's vertex count, and the batches must consume the table exactly. Batch lengths vary from lines and triangles through longer strips or polygons, so the portable model preserves the grouping instead of forcing all input into triangles prematurely.

Corpus shape and the executable's primitive dispatch distinguish the two observed
kinds. Kind 0 is a triangle strip with one independent draw per stored batch; its
303,484 batches contain at least three indexes. Kind 3 is a line list; all 157,860
of its batches contain exactly two indexes. The renderer binding flattens each
primitive's batches into one index buffer plus explicit draw ranges, preserving
strip boundaries and avoiding synthetic degenerate connectors. Across the corpus
this produces 57,284 triangle-strip primitives, 4,140 line-list primitives,
461,344 draw ranges, and the original 4,412,738 validated indexes.

## Safety and clean-room evidence

The parser caps files at 256 MiB, entries at 65,536, vertices per ordinary descriptor at 16,384, and topology tables at 1,048,576 words. It checks all arithmetic, duplicated offsets, descriptor extents, pointer alignment, complete vertex storage, finite components, topology extents, batch lengths, trailing words, and vertex references before constructing the model.

The complete supported corpus passes these rules. Private executable analysis independently confirms the descriptor size, vertex-count and data-offset accesses, 36-byte output stride, attribute positions, packed channel handling, and the high-bit handling used by packed index references. Public tests contain only an invented three-vertex fixture and destructive mutations; no retail vertex, index, or descriptor bytes are present in the repository.
