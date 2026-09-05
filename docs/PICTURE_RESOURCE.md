# PRM-backed UI picture-resource format

The retail window-picture loader resolves its serialized picture reference as a
byte displacement from the active scene PRM allocation. The renderer allocates
the exact raw PRM byte size plus a working reserve, reads the PRM member unchanged
at the allocation base, and retains that base for relocation lookups. The reserve
is renderer workspace and is not treated as retail input.

The picture reference is therefore neither a packed primitive index nor a TEX
identifier. `PictureResource::parse` accepts the bounded raw PRM bytes and the
recovered displacement. It never resolves against process memory or the renderer
reserve. The displacement must be two-byte aligned and the complete resource must
end before the PRM envelope's primitive-index boundary.

## Layout

At the referenced PRM-relative byte offset, the resource is:

| Relative field | Type | Meaning |
|---|---|---|
| 0 | `u32` | descriptor count |
| 4 | `descriptor[count]` | opaque 40-byte presentation descriptors |
| after descriptors | `u32` | frame count |
| after frame count | `u32[frame count]` | frame texture-resource references |
| after references | `frame[frame count]` | eight-byte frame records |

Each frame record contains one opaque 32-bit value followed by a 32-bit index
into the descriptor array. The retail loader rebases that index to a process
pointer in place; the portable model deliberately retains and validates the
index. The renderer associates `frame_texture_reference[i]` with `frame[i]`.
Presentation descriptor fields feed geometry and extents, not texture selection.

## Validation and ownership

The parser checks the PRM envelope and duplicated primitive-index boundary, the
relocation alignment and range, both count words, every count-by-stride
calculation, all array extents, and every descriptor index before allocating its
own values. Descriptors and frame data are copied, so no result points into the
source PRM buffer. The format has no proven resource-length word; `encoded_size`
reports the exact parsed prefix, and unrelated bytes before the primitive index
are allowed.

Project safety limits cap one picture resource at 4,096 descriptors and 4,096
frames. These are implementation resource limits, not claims about a retail
format maximum; the supported startup corpus ranges from 1 to 201 descriptors
and 1 to 159 frames.

Zero counts are structurally representable by the low-level parser. A higher-level
consumer may impose a non-empty requirement only when that invariant is proven
for its supported path.

Synthetic tests cover relocation, ownership, multiple descriptors and frames,
exact consumed size, unrelated trailing PRM bytes, zero counts, truncation at
every byte boundary, odd or out-of-range relocation keys, hostile counts, and
early and late invalid descriptor indexes. No retail
PRM bytes, picture references, descriptors, or texture references are present in
the repository.

Private compatibility validation joins every startup window-picture source to
the user-owned raw PRM and parses all 124 distinct picture resources. Installation
verification repeats that bounded join, requires 124 unique references, and does
not extract or log retail data.

## Remaining boundary

A frame texture-resource reference is another renderer relocation key. The clone
and rendering paths prove its role, but its target allocation has not yet been
tied bidirectionally to a raw TEX image or sequence. Until that producer and
consumer join is recovered, no frame reference may become a semantic UI texture
binding through names, dimensions, catalog order, numeric coincidence, or pixel
similarity. See the [retail UI texture boundary](RETAIL_UI_TEXTURES.md).
