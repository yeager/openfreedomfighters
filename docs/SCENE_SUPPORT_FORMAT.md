# Scene-support dependency format

Every scene archive in the supported Steam build contains one small `SUP` resource. The complete 90-file corpus validates as a `DLCF` dependency list: 47 files use a scalar layout and 43 use an array layout. No dependency-name inventory or retail payload is stored in this repository.

## Container

All integer fields are unsigned 32-bit little-endian values. Offsets below are absolute unless stated otherwise.

| Offset | Meaning | Validated value |
|---:|---|---|
| 0 | reserved | zero |
| 4 | root descriptor | file size with bit 31 set |
| 8 | file size | exact byte length |
| 12 | root item count | one |
| 16 | block tag | `DLCF` |
| 20 | block descriptor | low 30 bits equal file size minus 16 |

The top two descriptor bits select the observed payload representation:

- `00` is one NUL-terminated dependency string beginning at offset 24.
- `01` is a dependency array. Offset 24 contains the string-data offset relative to the `DLCF` tag, offset 28 contains the item count, and the length table begins at offset 32.

For the array representation, the relative string-data offset is exactly `16 + count * 4`. Each table length includes its terminating NUL. Concatenated strings are followed by zero to three zero alignment bytes. Empty string slots are valid and retained because one occurs in the supported corpus.

## Evidence and limits

The structural contract passes all 90 retail files. The largest observed list has 12 dependencies and the longest name has 16 bytes excluding its terminator. Private executable analysis independently confirms that the loader locates the `DLCF` tag, chooses the data start from the descriptor flags, obtains an item count, and walks NUL-terminated strings. The public implementation expresses only those interoperability facts.

The parser caps input at 1 MiB, dependency counts and individual fields at 4,096, accepts only the two observed layouts, checks all sizes and offsets before access, requires printable ASCII plus a final NUL, rejects embedded NUL bytes, and validates final alignment padding. Tests use invented dependency names only.
