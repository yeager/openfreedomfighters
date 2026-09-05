# Packed ZGF and GMS resource envelope

Every supported scene archive contains one `ZGF` and one `GMS` member. Both resource families use the same small outer envelope. This document covers only that envelope; the decompressed payload schemas remain separate research targets.

## Envelope

The two size fields are little-endian. Offsets are relative to the start of the archive member.

| Offset | Size | Meaning |
|---:|---:|---|
| 0 | 4 | exact decompressed payload size |
| 4 | 4 | exact packed member size, including this envelope |
| 8 | 1 | encoding: 0 for raw DEFLATE, 1 for stored bytes |
| 9 | variable | encoded payload |
| end - 4 | 4 | big-endian Adler-32 of the decompressed payload, encoding 0 only |

Encoding 0 contains an RFC 1951 raw DEFLATE stream without a zlib header. Its final four bytes use the checksum byte order of an RFC 1950 zlib trailer. The decoder requires the stream to consume the complete encoded range, produce exactly the declared payload size, and match the checksum.

Encoding 1 contains exactly the declared number of uncompressed bytes and has no checksum trailer. Unknown encodings, zero-length payloads, files whose declared size differs from their actual size, and payloads larger than 256 MiB are rejected.

## Corpus evidence

Installation verification decodes all 90 `ZGF` and all 90 `GMS` resources in the supported Steam build. Eighty-eight `ZGF` resources use raw DEFLATE and two use stored bytes; every `GMS` resource uses raw DEFLATE. Their decompressed sizes total 34,221,064 bytes for `ZGF` and 33,436,872 bytes for `GMS`.

Every decoded `ZGF` payload begins with the same four-byte inner signature. Its embedded-resource directory is parsed separately and documented in [ZGF_FORMAT.md](ZGF_FORMAT.md). The decoded `GMS` payload schema remains a research target.

## Validation coverage

Synthetic fixtures contain only project-authored text. Tests cover both encodings and reject mismatched packed or decompressed sizes, unknown encodings, damaged compressed data, invalid Adler-32 values, and trailing compressed bytes. No retail payload is stored in the repository or test suite.
