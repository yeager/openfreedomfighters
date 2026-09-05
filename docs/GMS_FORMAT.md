# GMS object-source image and runtime handles

Every scene archive contains one packed `GMS` resource. After outer-envelope decompression, it provides tables and source records used by the retail engine to populate runtime object pools. RMC/RMI geometry references address those runtime pools; they do not directly address bytes in the decoded GMS resource.

## Proven tables

The decoded image begins with a fixed 32-byte header. Header word 0 is the byte offset of an object-source directory, header word 1 is the byte offset of an identifier table, and header word 3 has the observed format value 4. Both table offsets are four-byte aligned.

The object-source directory begins with a 32-bit entry count followed by eight-byte entries:

| Entry offset | Size | Meaning |
|---:|---:|---|
| 0 | 4 | packed object-source reference |
| 4 | 4 | auxiliary value retained without guessed semantics |

Bits 23-0 of the packed reference are a word offset to a source record and therefore become a byte offset after multiplication by four. Bits 31-25 tell the loader how many parent pool contexts to leave before processing the entry. Bit 24 enters a child pool context after the current object is created. The loader reads a 48-byte source record; the public parser validates its complete range.

## Object-source records

Executable control flow establishes the resource domain and use of the following fields. Names remain deliberately conservative where the target structure is not decoded yet.

| Record offset | Type | Portable meaning |
|---:|---|---|
| 0 | `u32` | offset of the object's NUL-terminated name in the scene `BUF` resource |
| 4 | `u32` | GMS-relative offset of a nine-float object basis |
| 8 | `u32` | GMS-relative offset of a three-float object position |
| 12 | `u32` | class-specific value; a packed PRM primitive reference for 16 source types |
| 16 | `u32` | source type used by the object factory and pool classifier |
| 20 | `u32` | optional GMS-relative attachment-table offset |
| 24 | `u32` | object flags |
| 28 | `u32` | optional offset of an auxiliary block in `BUF` |
| 32 | `u32` | optional GMS-relative deferred-source offset |
| 36 | `u32` | child value retained without guessed semantics |
| 40 | `u32` | optional GMS-relative post-load-source offset |
| 44 | `u8` | reserved, zero throughout the supported corpus |
| 45 | `u8` | pool variant, observed as 0, 1, or 2 |

The attachment table starts with a 32-bit count followed by eight-byte entries. Each entry contains a GMS-relative source offset and a finite floating-point parameter. The retail loader passes that parameter through integer conversion when attaching the referenced source; the portable model preserves the original float until the target schema is known.

The parser decodes each basis and position to finite portable floats, checks every GMS-relative range, and retains the opaque values needed by later materialization. Executable control flow passes the first BUF reference to the common object constructor as a character pointer and stores it as the object's name. A separate cross-resource validator therefore checks both its range and NUL terminator. Across the 151,519 distinct source records, all referenced names terminate within their owning BUF; their lengths range from zero to 78 bytes.

Independent executable tracing establishes one additional behavioral boundary:
the loader copies the nine-float basis into runtime matrix storage, copies the
three-float position, and passes both to the transform setter of the
newly allocated runtime object before that object is registered. The loader also
maintains hierarchy context and creates attachment links, but the available
evidence does not establish their transform composition or coordinate spaces.

The current renderer's row-major `basis * local + position` calculation is an
explicit source-only diagnostic convention, not an established world-transform
formula. The parser does not infer hierarchy, parent-relative composition, map
composition, handedness, or normal transformation from these stored values. See
[TRANSFORM_BOUNDARY.md](TRANSFORM_BOUNDARY.md).

Optional auxiliary BUF blocks contain their bounded byte size in the low 30 bits of their second word; both their eight-byte header and complete declared extent are checked. There are 5,765 directory uses of auxiliary data, 5,433 among distinct source records, and 2,986 distinct block locations within their archives. Only ten of the 71 source types used by the corpus carry such blocks. The first header word is zero in every observed block. Payload meaning remains opaque pending type-specific decoding.

For 16 geometry source types, executable use and an exact corpus join establish record word 3 as a packed primitive reference. Zero means that the object has no direct primitive. Every one of the 115,977 present references across directory uses exactly matches an index value in the same archive's `PRM` catalog, including the high-bit reference form. The portable model exposes these references separately from the retained class-specific raw value, and installation verification rejects missing targets. Other source types use the same word for different class data and are not coerced into primitive references.

Window-picture sources use a separate class loader and do not carry a direct
primitive in that field. Their loader reads a picture asset reference from the
deferred class-specific serialization stream, resolves it through the texture
resource manager, and receives a frame count and frame descriptors. The public
parser does not yet expose that reference: tag-specific serialized lengths, the
base-class read sequence and version branch, and the manager key-space join must
be recovered first. The deferred offset remains bounds-checked, but the interval
between it and the generic source record is not treated as one opaque block
because other referenced data may occur there. See the
[retail UI texture boundary](RETAIL_UI_TEXTURES.md).

Across all 179,838 directory uses, 151,519 source-record offsets are distinct and 28,319 reuse an earlier record in the same image. There are 34,218 attachment tables containing 39,885 entries and 5,765 optional auxiliary BUF blocks. All referenced ranges and all transform and attachment floats validate. The two empty GMS images are also the only scene archives without `BUF`, proving the loader relationship without inventing placeholder data.

## Source-type registry

The supported executable exports 102 geometry class-info objects. Private control-flow analysis associates each export with its numeric source type and constructor entry point. The public portable registry retains only the interoperability pair of source type and exported class name; it contains all 102 entries, and every one of the 71 types present in the GMS corpus resolves uniquely. Unknown values remain representable and return no diagnostic class name instead of being coerced to a known type.

Header word 5 locates a third table. It begins with a pool-group count, followed by 24 little-endian class counts per group. A source-type field and one variant byte in each source record select its class. The directory's parent-step and child-entry operations select its group. Within each group, the loader allocates class ranges in ascending class order and assigns source records to consecutive 112-byte slots.

The portable parser reproduces this traversal and records each entry's group, class, class ordinal, group-relative slot index, and image-local slot index. It rejects hierarchy underflow, missing or extra groups, invalid classes, reused or unpopulated slots, and any mismatch between observed assignments and declared class counts. Across the supported corpus, all 179,838 assignments exactly reproduce all 24 counters in all 29,450 groups; no inferred correction or fallback is needed.

The identifier table also begins with a 32-bit count. Each following 32-bit value is a byte offset to a NUL-terminated identifier in the same decoded image. The public parser validates every range and terminator but deliberately exposes only the count, not retail identifier text.

Across the 90 supported images, the parser also validates 154,941 identifier references. Their decoded size totals 33,436,872 bytes.

## Runtime object handles

A present RMC/RMI geometry handle is a little-endian 32-bit value:

| Bits | Meaning |
|---:|---|
| 31-30 | required tag `01` |
| 29-0 | byte offset in a runtime object pool |

Executable analysis shows the loader allocating objects in 112-byte (`0x70`) slots and constructing handles by combining a slot offset with tag `01`. The tagged value `0x40000000` therefore identifies runtime slot zero; it is distinct from the absent optional value zero.

Corpus validation confirms that all 2,801 primary handles and all 201 present optional handles use this tag and alignment. There are 835 archive-local distinct slot offsets; repeated handles account for the remaining 2,167 descriptor uses. The reproduced allocation order maps 2,998 handle uses to local source-directory entries. The remaining four are slot-zero handles in two images whose local source directories are empty, so they are explicitly retained as external rather than assigned a guessed source.

## Validation coverage

Synthetic tests use a project-authored stored image. They cover directory, pool-count, identifier, transform, attachment, and BUF-relative bounds; finite transform decoding; object-name termination; auxiliary BUF extents; source-type diagnostics; parent/child pool traversal; class and local-slot assignment; local and external handle lookup; packed source-reference fields; tagged slot-zero and interior runtime handles; unsupported tags; and handle misalignment. No retail GMS, BUF, or identifier content is present in the repository.
