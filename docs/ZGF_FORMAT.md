# ZGF resource bundle format

The decoded `ZGF` payload is a bounds-checked bundle of named embedded resources. The portable reader preserves embedded bytes and names without extracting them to the host filesystem.

## Root envelope

All integers are little-endian and all offsets are relative to the start of the decoded `ZGF` payload.

| Offset | Size | Meaning |
|---:|---:|---|
| 0 | 4 | `TFGZ` signature (`0x5a474654`) |
| 4 | 4 | decoded size with tag bits `10` |
| 8 | 4 | decoded size without tag bits |
| 12 | 4 | entry count |
| 16 | variable | consecutive entry records |

The 30 low bits of the tagged size equal the complete decoded payload size. The entry chain must cover the payload exactly. Two resources in the supported corpus use an eight-byte empty form containing only the signature and size.

## Entry record

| Offset | Size | Meaning |
|---:|---:|---|
| 0 | 4 | observed entry type, always 1 |
| 4 | 4 | complete entry size with tag bits `10` |
| 8 | 4 | offset of the logical resource name |
| 12 | 4 | embedded-blob count, always 1 |
| 16 | variable | embedded-blob record followed by the name |

An embedded-blob record begins with its exact payload size and an untagged record size. The record contains an eight-byte header, the payload, and zero padding to four-byte alignment. Its end must equal the entry's name offset.

The remaining entry bytes contain one printable ASCII, NUL-terminated logical path and zero alignment padding. These identifiers are never used as host filesystem paths. Parent segments are retained because they are present in valid logical references; absolute paths, drive-qualified paths, empty components, and single-dot components are rejected. Names must be unique after ASCII case folding and slash normalization.

## Embedded resource families

The 430 bundled `TTF` payloads begin with the standard TrueType sfnt signature and are fonts. The 589 `PPO` payloads are compiled script objects whose internal schema remains a separate research target.

RMC/RMI geometry references are not ZGF bundle offsets or raw GMS byte offsets. They identify 112-byte runtime object-pool slots populated from GMS object-source records. See [GMS_FORMAT.md](GMS_FORMAT.md).

## Validation coverage

Installation verification parses 90 bundles containing 1,019 entries and 34,161,792 embedded payload bytes. The observed logical resource extensions comprise 430 `TTF` and 589 `PPO` entries. Synthetic tests use only project-authored names and bytes and reject invalid signatures, root sizes, entry types, blob sizes, alignment padding, names, and out-of-range access.
