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
| 4 | `descriptor[count]` | 40-byte presentation descriptors |
| after descriptors | `u32` | ordered draw-group count |
| after draw-group count | `u32[draw-group count]` | draw-group texture-resource references |
| after references | `draw_group[draw-group count]` | eight-byte draw-group records |

Each draw-group record contains a 32-bit descriptor span count followed by the
32-bit index of its first descriptor. The retail loader rebases that index to a
process pointer in place; the portable model deliberately retains and validates
the index. The renderer associates `texture_reference[i]` with `draw_group[i]`
and submits every group in serialized order. These records do not select an
animation frame or UI state.

Each descriptor is ten little-endian words: local centre X, local centre Y,
local Z, U minimum, U maximum, V maximum, V minimum, horizontal edge span,
vertical edge span, and packed modulation colour. All fields except colour are
IEEE-754 binary32. A neutral quad uses `centre +/- 0.5 * edge_span`, preserves
the serialized UV endpoints and local Z, and carries the packed colour unchanged.
The renderer-neutral plan names those bounds `local_x_min`, `local_x_max`,
`local_y_min`, and `local_y_max`; the names intentionally make no screen-axis
or transform-orientation claim.
The owning window/scene transform is external and remains the caller's
responsibility.

Each draw-group texture-resource reference is itself an unsigned PRM-relative byte
displacement. It resolves to a neutral 32-byte typed resource record embedded
in the bounded PRM data region. The portable model retains the displacement and
owns an opaque copy of the complete record; it never retains a process pointer.
The record's type marker is validated, but its remaining fields deliberately
have no public semantic names yet. Retail materialization consumes record fields
and replaces copied draw-group references with renderer handles; the producer
semantics remain unresolved, so raw values must not be interpreted as TEX
identities.

## Validation and ownership

The parser checks the PRM envelope and duplicated primitive-index boundary, the
relocation alignment and range, both count words, every count-by-stride
calculation, all array extents, and every descriptor span before allocating its
own values. Descriptor floats must be finite and edge spans non-negative. Every
draw-group texture-resource displacement is independently checked
for two-byte alignment, a complete 32-byte extent before the primitive-index
boundary, and the recovered type marker. Descriptors, frame data, and resolved
opaque resource records are copied, so no result points into the source PRM
buffer. Multiple draw groups may safely share one valid resource displacement. The
format has no proven resource-length word; `encoded_size`
reports the exact parsed prefix, and unrelated bytes before the primitive index
are allowed.

Project safety limits cap one picture resource at 4,096 descriptors and 4,096
draw groups. These are implementation resource limits, not claims about a retail
format maximum; the supported startup corpus ranges from 1 to 201 descriptors
and 1 to 159 draw groups.

Zero counts are structurally representable by the low-level parser. A higher-level
consumer may impose a non-empty requirement only when that invariant is proven
for its supported path.

Synthetic tests cover relocation, ownership, descriptor decoding and draw groups,
exact consumed size, unrelated trailing PRM bytes, zero counts, truncation at
every byte boundary, odd or out-of-range relocation keys, hostile counts, and
early and late invalid descriptor spans, hostile arithmetic, zero-length bounded
spans, non-finite floats, and negative edge spans. Draw-group resource coverage additionally
includes odd, header-relative, boundary-truncated, and wrong-type references,
owned-record lifetime, and a legal shared reference. No retail
PRM bytes, picture references, descriptors, or texture references are present in
the repository.

Private compatibility validation joins every startup window-picture source to
the user-owned raw PRM and parses all 124 distinct picture resources. Installation
verification repeats that bounded join, requires 124 unique picture references,
resolves all 1,144 draw-group resource records, requires each picture's groups
to form an ordered gap-free descriptor partition, and joins them to 334 distinct startup
TEX images. Its aggregate corpus gate records no offsets, identifiers, or retail
bytes.

## Texture-manager key join

The typed record's little-endian word at byte offset four is an unsigned manager
key. The manager has 2,048 image slots and two key banks: keys below 2,048 select
the same-numbered slot, while keys from 2,048 through 4,095 select the slot after
subtracting 2,048. The startup picture path requires the upper bank. Its paired
TEX catalog has no sequences, so the consumer's selection zero resolves directly
to the image whose ID equals the normalized slot. Every binding is checked in
reverse before use. Out-of-range keys, missing images, cross-bank aliases, and
sequence-bearing slots fail closed; behavior for sequence-bearing scenes is not
claimed without matching corpus validation.

`PictureTextureBindings` owns its neutral binding records; their image indexes
remain meaningful only for the catalog used to build them. Callers must retain
that catalog and invalidate bindings on scene replacement. This identity join
does not assign semantic UI roles. See the [retail UI texture boundary](RETAIL_UI_TEXTURES.md).
