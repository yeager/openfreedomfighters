# RMC and RMI spatial-map format

Every supported scene archive contains one `RMC` and one `RMI` member. Both use the same validated envelope: a spatial header and packed hierarchy, an array of quantized bounds and descriptor references, and a fixed-size object-descriptor region. Their exact runtime distinction is not yet established, so the parser deliberately shares one neutral data model.

## File envelope

All integers and floats are little-endian. Offsets are relative to the start of the member.

| Offset | Size | Meaning |
|---:|---:|---|
| 0 | 4 | index-table offset |
| 4 | 16 | four finite root-space parameters |
| 20 | 4 | quantization scale; `0x00010000` in the supported corpus |
| 24 | 4 | hierarchy flags, preserved without interpretation |
| 28 | 4 | hierarchy parameter, preserved without interpretation |
| 32 | variable | packed hierarchy bytes |
| index offset | `16 * count` | spatial index records |
| descriptor start | `84 * count` | object descriptors |

The bytes following the index offset are exactly divisible into 100 bytes per entry: a 16-byte index record and one 84-byte descriptor. Consequently, the entry count and descriptor boundary can be derived without trusting an embedded count. Every descriptor slot must be referenced exactly once.

## Spatial index record

| Offset | Size | Meaning |
|---:|---:|---|
| 0 | 4 | object-descriptor offset |
| 4 | 6 | three unsigned 16-bit quantized minimum coordinates |
| 10 | 6 | three unsigned 16-bit quantized maximum coordinates |

For all 2,801 records in the supported installation, each minimum coordinate is less than or equal to its corresponding maximum. Descriptor references can be permuted; file order is therefore not assumed to equal index order.

## Object descriptor

| Offset | Size | Portable model |
|---:|---:|---|
| 0 | 4 | object kind, observed values 0 and 1 |
| 4 | 36 | 3-by-3 orientation matrix |
| 40 | 12 | position vector |
| 52 | 12 | auxiliary position vector |
| 64 | 12 | extents vector |
| 76 | 8 | two scale terms |

All 20 floating-point values are required to be finite. The matrix/vector names describe the strongly observed numeric shape and are sufficient for a portable spatial model; exact engine semantics, especially the auxiliary vector and scale terms, remain open research questions.

## Validation coverage

Installation verification parses 90 `RMC` files containing 1,612 entries and 90 `RMI` files containing 1,189 entries. Synthetic tests reject truncated or misaligned envelopes, unsupported quantization scales and kinds, duplicate or misaligned descriptor references, inverted bounds, and non-finite values. Packed hierarchy bytes are retained but not traversed until their encoding is independently established.
