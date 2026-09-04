# Audio-bank header format

The supported Steam build contains 45 loose `WHD` metadata files paired with local `WAV` banks, plus one global stream bank. The native parser models only structural and interoperability facts established across the complete user-owned corpus.

## Confirmed layout

Every observed header has the same byte layout:

| Region | Size | Confirmed contract |
|---|---:|---|
| Header | 16 bytes | words 0 and 1 are file size minus 8 and full file size; words 2 and 3 are `3` and `4` |
| Record table | 48 bytes per record | 12 little-endian 32-bit words; the first two are always `6` and `0` |
| Footer | 8 bytes | both words are zero |

All 45 headers satisfy `(file_size - 24) % 48 == 0`, yielding 121,187 records. The bounds-checked C++ parser rejects inconsistent declared sizes, truncated record tables, invalid record markers, implausible channel/rate/bit metadata, empty payloads, and nonzero footer words.

## Record fields

The following interpretations are supported by corpus-wide relations:

| Word | Public model | Evidence |
|---:|---|---|
| 2 | format and storage flags | only four values occur; bit 31 selects the global bank |
| 3 | sample rate | only 11,025, 22,050, and 44,100 occur |
| 4 | bits per sample | only 4 and 16 occur |
| 5 | value count | semantics remain provisional |
| 6 | encoded byte size | all resulting byte ranges fit their selected bank |
| 7 | channel count | only mono and stereo occur |
| 8 | payload byte offset | all resulting byte ranges fit their selected bank |
| 9 | frame-related count | semantics remain provisional |
| 10 | block alignment | consistent with the format families below |
| 11 | samples per block | consistent with the format families below |

The `0x11` family uses 4-bit mono blocks. Its 512-byte/1,017-sample and 256-byte/505-sample pairs satisfy the standard IMA ADPCM samples-per-block equation, making IMA ADPCM a high-confidence decoding hypothesis. The `0x80000001` family has PCM-like 16-bit mono/stereo alignment. The `0x80001000` family remains deliberately unnamed until decoded audio is compared privately against a reference.

Bit 31 is a global-bank selector: all 111,264 `0x80000011`, 379 `0x80000001`, and 2,051 `0x80001000` records fit the global stream bank. All 7,493 records without bit 31 fit their paired local banks. No payload bytes are included in the repository or test fixtures.

## Runtime validation

Installation verification parses all 45 headers, requires the expected aggregate record count, opens every paired local bank and the global bank as streaming views, and validates every declared byte range without decoding or copying audio into the repository.
