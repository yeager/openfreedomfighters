# Resource-format census

The census reads at most 32 uncompressed bytes from each archive member and emits aggregate measurements only. It does not extract game data. Results below describe the supported Steam build identified in [TECHNICAL_MAP.md](TECHNICAL_MAP.md).

## Corpus

- 90 valid ZIP archives; 0 invalid.
- 1,118 members in 13 resource families.
- 89 archives append a 22-byte engine footer after the standard ZIP end record; one has no trailing engine footer.
- ZIP methods observed by the tool are recorded numerically so results remain independent of host library labels.

| Format | Count | Minimum bytes | Median bytes | Maximum bytes | Stable leading bytes |
|---|---:|---:|---:|---:|---|
| `ANM` | 42 | 72,936 | 1,740,520 | 3,324,532 | `4d 4e 41 00` (`MNA\0`) |
| `BUF` | 88 | 208 | 16,392 | 677,844 | none |
| `GMS` | 90 | 5,905 | 43,500 | 380,622 | none |
| `LOC` | 88 | 30,570 | 98,210 | 135,323 | none |
| `OCT` | 90 | 100 | 20,852 | 1,339,596 | none; only two distinct 16-byte prefixes |
| `PRM` | 90 | 76,892 | 808,786 | 12,741,342 | `bc 2a 01 00` (`76476` LE) |
| `RMC` | 90 | 132 | 132 | 20,032 | none |
| `RMI` | 90 | 132 | 132 | 18,736 | none |
| `SGP` | 90 | 16 | 16 | 155,326 | none; 69 are zero-leading |
| `SND` | 90 | 64 | 465 | 444,567 | none |
| `SUP` | 90 | 36 | 36 | 236 | four zero bytes |
| `TEX` | 90 | 16,400 | 2,591,207 | 19,633,453 | none |
| `ZGF` | 90 | 17 | 162,752 | 372,438 | none |

`ANM` likely stores its ASCII signature in little-endian convention. The constant `PRM` first word is more likely a version, fixed base/header size, or root offset than a four-character code. These are hypotheses, not parser contracts yet.

## Three-scene consistency probe

The smallest loader scene, the startup scene, and the first campaign scene were compared. These cover a minimal package, a UI-heavy package, and a geometry/animation-heavy package.

Confirmed structural invariants (32-bit words are zero-indexed):

- All three archives contain the same 12 base resource families. The campaign scene adds `ANM`.
- `ZGF` word 1 exactly equals total `ZGF` byte length in all three.
- `SUP` word 0 is zero and word 2 equals total `SUP` byte length in all three.
- `GMS` word 1 exactly equals total `GMS` byte length in all three.
- `TEX` word 0 equals total length minus 16,384 bytes; word 1 equals word 0 plus 8,192; words 2 and 3 are consistently `3` and `4`.
- `SND` word 0 equals total length minus 48 bytes; word 1 equals total byte length; words 2 and 3 are consistently `3` and `4`.
- `PRM` word 0 is always `76,476`, including all 90 files. In the three-scene sample words 1 and 2 are equal, but their relation to total length varies and needs section-table analysis.
- The sampled `ANM` begins with `MNA\0`, has total byte length in word 2, then values `12` and `10` in words 3 and 4. All 42 `ANM` files share only the four-byte signature, so later header fields vary.
- `RMC` and `RMI` are byte-identical in both loader/startup scenes (132 bytes each) but diverge in the campaign scene. This supports paired render metadata with a shared empty/default representation.

The rules above were then promoted to validators and checked against the full corpus. `ZGF`, `SUP`, `GMS`, `SND`, `PRM`, and `ANM` pass every stated rule in every observed file. `TEX` passes the size and `3,4` rules in all 90 files; word 1 has an 88-file normal variant (`word 0 + 8,192`) and a two-file empty variant (`word 0`). The next step is to locate section offsets from these length equations and test mutations against synthetic fixtures.

## Reproduce

```sh
python3 tools/resource_census.py /path/to/FreedomFighters
```

Do not commit output produced with local filenames or extracted payloads.
