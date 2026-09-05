# RMC and RMI spatial-map format

Every supported scene archive contains one `RMC` and one `RMI` member. Both use the same validated envelope: a quantized octree, an array of bounds and descriptor references, and a fixed-size object-descriptor region. Their exact runtime distinction is not yet established, so the parser deliberately shares one neutral data model.

## File envelope

All integers and floats are little-endian. Offsets are relative to the start of the member.

| Offset | Size | Meaning |
|---:|---:|---|
| 0 | 4 | index-table offset |
| 4 | 12 | finite world-space octree center |
| 16 | 4 | positive floating-point world-to-quantized-space factor |
| 20 | variable | packed 6-byte octree nodes followed by less than 16 bytes of alignment padding |
| index offset | `16 * count` | spatial index records |
| descriptor start | `84 * count` | object descriptors |

The bytes following the index offset are exactly divisible into 100 bytes per entry: a 16-byte index record and one 84-byte descriptor. Consequently, the entry count and descriptor boundary can be derived without trusting an embedded count. Every descriptor slot must be referenced exactly once.

## Packed octree node

The node array begins at offset 20 and node zero is the root. Nodes are addressed by array index, not byte offset.

| Offset | Size | Meaning |
|---:|---:|---|
| 0 | 2 | packed flags and element count |
| 2 | 2 | first-child node index, or zero when there are no children |
| 4 | 2 | first spatial-index element |

Bits 0-2 of the packed word select an octant. Bits 3-14 hold the number of consecutive elements owned by the node, and bit 15 marks the final node in a contiguous sibling list. A nonzero child index selects the first sibling; traversal continues through consecutive nodes until the final-sibling bit is encountered.

The parser follows the tree from node zero and requires every referenced node to be in range and reachable exactly once. Sibling octants must be unique, and depth cannot exceed the 16 levels representable by the quantized coordinate space. Node element ranges must remain inside the spatial index and cover every element exactly once without overlap. This traversal also determines the true end of the node array, separating it from alignment bytes without interpreting padding as nodes.

## World-space queries

The portable reader performs bounds queries with the same quantized loose-octree rules recovered from the Windows executable. Each world coordinate is converted with `trunc((world - center) * factor + 0.5)`. The converted maximum is incremented before traversal, unless it is already saturated at the signed 32-bit maximum.

The root center is `(0x8000, 0x8000, 0x8000)`. At child depth `d`, the cell size is `0x10000 >> d`; each child center moves by half that size according to its three octant bits. A child is traversed when the query overlaps its loose bounds of `center +/- cell size`. Spatial-index records use half-open intersection tests: a query axis must extend below the record maximum and above the record minimum.

Traversal is iterative and preserves packed sibling order. It visits only intersecting child cells and returns the original spatial-index positions, allowing later rendering and collision systems to resolve descriptors without copying game data.

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
| 76 | 4 | required packed primary geometry reference |
| 80 | 4 | optional packed secondary geometry reference |

All 18 floating-point values are required to be finite. The matrix/vector names describe the strongly observed numeric shape and are sufficient for a portable spatial model; exact engine semantics, especially the auxiliary vector, remain an open research question.

Executable query callbacks independently establish that offset 76 is passed to the engine's geometry-object lookup and offset 80 selects an additional object for the relevant descriptor form. Every primary reference in the supported corpus has tag bits `01` and a 112-byte-aligned 30-bit runtime-pool offset. Secondary references use the same representation when present; zero means absent. All 1,612 `RMC` descriptors have no secondary reference, while 201 of 1,189 `RMI` descriptors have one. The tagged value `0x40000000` identifies runtime slot zero and is distinct from an absent optional reference. These handles do not directly address bytes in the GMS resource. See [GMS_FORMAT.md](GMS_FORMAT.md).

## Validation coverage

Installation verification parses 90 `RMC` files containing 2,587 octree nodes and 1,612 entries, plus 90 `RMI` files containing 1,359 nodes and 1,189 entries. Synthetic tests exercise whole-tree and octant-selective world queries and reject invalid query bounds, truncated or misaligned envelopes, invalid quantization factors, cyclic or reused nodes, repeated sibling octants, overlapping element ownership, excessive hierarchy padding, unsupported object kinds, duplicate or misaligned descriptor references, invalid geometry references, inverted bounds, and non-finite values.
