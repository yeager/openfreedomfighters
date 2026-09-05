# Texture-catalog format

Each of the 90 scene archives in the supported Steam build contains one `TEX` catalog. The native parser validates all 23,522 image records and 19 image-sequence records without exporting retail names or pixel data.

## Envelope and indexes

All integer fields are unsigned 32-bit little-endian values. A catalog begins with four words:

| Offset | Meaning | Corpus contract |
|---:|---|---|
| 0 | end of the variable data area and start of the image index | file size minus 16,384 |
| 4 | start of the sequence index | normally offset 0 plus 8,192 |
| 8 | format value | 3 |
| 12 | format value | 4 |

Both indexes have 2,048 entries. A nonzero image-index entry maps its numeric image ID to the absolute offset of that image's block descriptor. A nonzero sequence-index entry maps its sequence ID to a sequence block. Every entry is cross-checked in both directions: unindexed blocks, dangling offsets, duplicate image IDs, and missing image references are rejected.

Two catalogs contain no image blocks. In this empty variant both offsets are 16 and share one all-zero index. The final unused 8 KiB region contains non-semantic bytes that the parser deliberately ignores and never exposes.

## Image blocks

An image block begins with a 32-bit byte length that includes the descriptor itself. The high two bits are zero in every observed block. Its fixed fields contain:

- two matching format tags;
- an image ID;
- 16-bit width and height packed into one word;
- a mip-level count;
- three preserved metadata words whose semantics are not yet claimed;
- a printable ASCII, NUL-terminated resource name.

Four format tags occur in the corpus:

| Tag | Portable model | Stored bytes per mip |
|---|---|---|
| `DXT1` | BC1-compatible block data | 8 bytes per 4 by 4 block |
| `DXT3` | BC2-compatible block data | 16 bytes per 4 by 4 block |
| `ABGR` | four-byte color data | width times height times 4 |
| `PALN` | 8-bit palette indexes | width times height, followed by a palette |

Every mip is prefixed by its encoded byte count. Dimensions halve at each level and clamp to one. The parser independently derives the required size for every level and rejects mismatches. A `PALN` block ends with a palette count and one 32-bit value per entry; the observed palette range is 1 through 256 entries.

## Portable pixel decoding

The native CPU decoder converts every observed encoding to tightly packed RGBA8. `DXT1` follows BC1 color interpolation and transparent-selector rules. `DXT3` follows BC2 four-color interpolation and expands each explicit 4-bit alpha value to eight bits. Edge blocks are clipped to the declared mip dimensions.

Uncompressed `ABGR` pixels are stored in byte order blue, green, red, alpha and are swizzled to RGBA8. Each `PALN` byte indexes a packed palette color with masks `0xff000000` for alpha, `0x00ff0000` for red, `0x0000ff00` for green, and `0x000000ff` for blue. The parser rejects an index outside the image's declared palette before decoded output can be requested.

Invented golden vectors cover color interpolation, DXT1 transparency, DXT3 alpha expansion, channel order, palette expansion, clipped blocks, and malformed storage. Installation verification also decodes one user-owned mip from each encoding family during the complete corpus pass and discards the pixels immediately.

## Image sequences

Sequence blocks contain a count followed by image IDs. Their owning sequence ID comes from the second index. All referenced IDs must exist in the image index, and the indexed sequence ID must occur in its own list. Repeated frame IDs remain valid because they encode timing through repetition in observed lists.

## Safety and clean-room evidence

The parser caps files at 128 MiB, dimensions at 4,096, mip chains at 16 levels, names and individual sequence lists at 4,096 entries, palettes at 256 entries, and indexed objects at 2,048. The decoder independently caps output at 16 million pixels. Every offset, multiplication, block length, mip size, string terminator, palette, and reference is checked before use.

Corpus-derived size equations and index relationships pass all 90 retail files. Private executable analysis independently branches on the same four format tags, exposes matching bitmap-class boundaries, and confirms the packed channel masks. Only interoperability facts and invented test fixtures are present in the public repository.
