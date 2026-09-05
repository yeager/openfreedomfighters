# GMS object-source image and runtime handles

Every scene archive contains one packed `GMS` resource. After outer-envelope decompression, it provides tables and source records used by the retail engine to populate runtime object pools. RMC/RMI geometry references address those runtime pools; they do not directly address bytes in the decoded GMS resource.

## Proven tables

The decoded image begins with a fixed 32-byte header. Header word 0 is the byte offset of an object-source directory, header word 1 is the byte offset of an identifier table, and header word 3 has the observed format value 4. Both table offsets are four-byte aligned.

The object-source directory begins with a 32-bit entry count followed by eight-byte entries:

| Entry offset | Size | Meaning |
|---:|---:|---|
| 0 | 4 | packed object-source reference |
| 4 | 4 | auxiliary value retained without guessed semantics |

Bits 23-0 of the packed reference are a word offset to a source record and therefore become a byte offset after multiplication by four. Bits 31-25 select a hierarchy depth used by the loader, while bit 24 is retained as a separate flag. The loader reads at least 48 bytes from each referenced source record; the public parser validates that complete minimum range.

The identifier table also begins with a 32-bit count. Each following 32-bit value is a byte offset to a NUL-terminated identifier in the same decoded image. The public parser validates every range and terminator but deliberately exposes only the count, not retail identifier text.

Across the 90 supported images, the parser validates 179,838 object-source entries and 154,941 identifier references. Their decoded size totals 33,436,872 bytes.

## Runtime object handles

A present RMC/RMI geometry handle is a little-endian 32-bit value:

| Bits | Meaning |
|---:|---|
| 31-30 | required tag `01` |
| 29-0 | byte offset in a runtime object pool |

Executable analysis shows the loader allocating objects in 112-byte (`0x70`) slots and constructing handles by combining a slot offset with tag `01`. The tagged value `0x40000000` therefore identifies runtime slot zero; it is distinct from the absent optional value zero.

Corpus validation confirms that all 2,801 primary handles and all 201 present optional handles use this tag and alignment. There are 835 archive-local distinct slot offsets; repeated handles account for the remaining 2,167 descriptor uses. Mapping a handle back to its source record requires reproducing the loader's 24-class pool-allocation order and remains the next research step.

## Validation coverage

Synthetic tests use a project-authored stored image. They cover directory and identifier-table bounds, packed source-reference fields, tagged slot-zero and interior runtime handles, unsupported tags, and handle misalignment. No retail GMS content or identifier text is present in the repository.
