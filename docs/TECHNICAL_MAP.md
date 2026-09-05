# Technical map: Steam PC build

## Observed installation

Local source: Steam 2020 Windows re-release, with a game executable timestamped January 2021. The repository records only non-expressive facts. See [BUILD_PROVENANCE.md](BUILD_PROVENANCE.md) for the evidence separating original content from the later Steam/Windows build.

| Property | Observation |
|---|---|
| Installation size | approximately 636 MiB |
| File count | 188 |
| Main executable | PE32, Intel i386, Windows GUI |
| Main executable size | 3,353,592 bytes |
| Main executable SHA-256 | `b05ca73c44474320e1b7321c24be66270f1de2a0686b063b6cb18e9ed21de9c9` |
| Engine identity | Glacier |
| Graphics API | Direct3D 8 |
| Input | DirectInput 8 |
| Audio | DirectSound with optional EAX/OpenAL paths |
| Distribution integration | Steam API |
| Content layout | 90 ZIP scene packages, 46 WAV banks, 45 WHD headers |
| Archive contents | 1,118 members; 1,005,021,227 bytes uncompressed |
| Campaign layout | startup, intro, cutscenes, and single-player scene groups |

The executable contains developer-facing diagnostics and source-path identifiers that expose subsystem boundaries: filesystem/ZIP, save games, renderer, DirectInput, audio/EAX, scripting host, GUI, player profile, mission state, and Steam lifecycle. These names are evidence for decomposition only and must not be copied as original implementation.

## Scene-package anatomy

Representative scene archives consistently expose a family of resource types:

| Extension | Working hypothesis | Verification needed |
|---|---|---|
| `ZGF` | packed bundle of named scene resources | embedded `TTF`/`PPO` schemas and cross-bundle references |
| `SUP` | `DLCF` dependency list | downstream loading semantics |
| `BUF` | binary data buffers | offsets, alignment, ownership |
| `GMS` | decoded scene image addressed by RMC/RMI geometry references | inner section, relocation, and object semantics |
| `TEX` | texture catalog with indexed image and sequence blocks | renderer upload and material references |
| `SND` | sound event metadata | bank links, loop and spatial flags |
| `LOC` | localization table | encoding, identifiers, plural rules |
| `OCT` | spatial octree | bounds and cell/object references |
| `SGP` | scene/game parameters | determine semantics |
| `RMC`/`RMI` | validated quantized octrees, bounds, and fixed object descriptors | exact runtime distinction and binding to scene objects |
| `PRM` | indexed render primitives with decoded primary vertices and grouped topology | materials and auxiliary/skinning streams |
| `ANM` | animation data (42 observed) | clips, tracks, time units, skeleton binding |

This table is provisional. Every claim graduates only after corpus-wide validation and a synthetic parser test.

Aggregate sizes, header invariants, and the first three-scene comparison are recorded in [FORMAT_CENSUS.md](FORMAT_CENSUS.md).

Across all 90 archives, each of `GMS`, `OCT`, `PRM`, `RMC`, `RMI`, `SGP`, `SND`, `SUP`, `TEX`, and `ZGF` occurs 90 times. `BUF` and `LOC` occur 88 times and `ANM` occurs 42 times. This regularity strongly supports a per-scene resource-family design. The common `ZGF`/`GMS` compression envelope and the inner ZGF resource bundle are decoded, `SUP` is confirmed as a dependency list, `TEX` is decoded to RGBA8, `PRM` primary vertices and grouped topology are decoded, and the common `RMC`/`RMI` spatial envelope is parsed; the remaining family semantics are still hypotheses.

## PE image map

The supported executable is PE32/i386, image base `0x00400000`, entry-point RVA `0x00233903`, with relocations present. Its seven sections are:

| Section | RVA | Virtual bytes | File offset | File bytes |
|---|---:|---:|---:|---:|
| `.text` | `0x00001000` | 2,574,008 | 1,024 | 2,574,336 |
| `.rdata` | `0x00276000` | 449,620 | 2,575,360 | 450,048 |
| `.data` | `0x002e4000` | 1,245,772 | 3,025,408 | 101,888 |
| `.gfids` | `0x00415000` | 48 | 3,127,296 | 512 |
| `.tls` | `0x00416000` | 9 | 3,127,808 | 512 |
| `.rsrc` | `0x00417000` | 3,904 | 3,128,320 | 4,096 |
| `.reloc` | `0x00418000` | 213,240 | 3,132,416 | 213,504 |

The large zero-filled tail of `.data`, thread-local storage, and relocation table matter for any static-lifting runtime. Import-table parsing and TLS callback discovery are still required before choosing a lifting ABI.

### Loader directories and exported registry

The image has export, import, resource, certificate, relocation, debug, TLS, load-config, and import-address-table directories. It has no delay-import or x86 exception directory. The TLS directory has no callback array, so the normal entry point is the first observed executable initialization boundary. Authenticode uses a file-offset certificate entry, as required by the PE format.

All 341 exports are named:

- 102 geometry/object class-info records;
- 230 routed event/behavior class-info records;
- one additional decorated WinMain property record;
- eight plain engine boundary functions covering engine execution, renderer creation, project interface, script engine/interfaces, editing output, and destruction notification.

This is strong evidence that Glacier uses exported static class metadata to register scene objects and routed behaviors. The registry gives Phase 3 a finite compatibility surface: decode class identifiers from scene data, map them to behavior specifications, then implement only the classes exercised by the campaign vertical slice before expanding to all 332 records.

## Imported platform surface

The executable imports 274 symbols from 24 DLLs. The narrow multimedia boundary is encouraging: one Direct3D 8 factory call, one DirectInput 8 factory call, one XInput state call, one ordinal DirectSound entry, and one ordinal EAX entry. Steam integration uses only init, shutdown, and restart-if-needed imports. Winsock contributes 11 ordinal imports.

The remaining surface is chiefly Win32 process/thread synchronization, files, virtual memory, timing, window/message handling, GDI queries, user identity, COM, GDI+ image loading, and the Visual C++ runtime. This suggests the platform shim can remain small, but ordinal resolution and dynamically loaded APIs must be mapped before that conclusion is final.

| DLL family | Imported symbols | Replacement direction |
|---|---:|---|
| Steam API | 3 | optional ownership/platform adapter |
| Direct3D 8 | 1 | native render backend |
| DirectInput 8 + XInput | 2 | SDL input/action layer |
| DirectSound + EAX | 2 | portable mixer/spatial audio |
| Winsock 2 | 11 | determine whether runtime-critical; portable sockets if needed |
| Kernel/User/GDI/Shell/COM/GDI+ | 149 | platform, filesystem, threading, window, and image shims |
| Visual C++ runtime families | 106 | native standard/runtime equivalents |

The import totals above are generated by `tools/inspect_install.py`; DLL names and counts are interoperability facts, not original code.

## Audio

Files named `.WAV` are often banks rather than conventional RIFF WAV files. All 45 paired `.WHD` headers now parse as a fixed header, 48-byte stream records, and a fixed footer; every declared payload range fits either its local bank or the global stream bank according to a storage flag. The native layer decodes the three observed families: signed 16-bit PCM, Microsoft-layout IMA ADPCM, and Ogg Vorbis. See [AUDIO_FORMAT.md](AUDIO_FORMAT.md).

## Key unknowns

- PE image sections, import table, calling conventions, and code/data relocation model.
- Exact Glacier resource schemas and archive precedence rules.
- Script representation and VM semantics.
- Fixed-step simulation frequency, frame-dependent behavior, and deterministic state.
- Skeleton, animation, AI navigation, squad command, combat, and camera rules.
- Save/profile schema and compatibility expectations.
- Original localization identifiers and safe translation workflow.
- macOS arm64 and Linux x86-64/arm64 strategy for any translated x86 behavior.

## Next probes

1. Resolve runtime-computed dynamic module/API arguments and map load-config metadata.
2. Infer GMS section boundaries and identify the object records reached by RMC/RMI geometry references.
3. Determine the `RMC`/`RMI` runtime distinction and bind resolved GMS objects to `PRM`, then bind decoded `TEX` pixels to renderable geometry.
4. Record black-box boot, menu, input, timing, and first-level traces from the retail game.
5. Define golden screenshots/state traces stored locally as hashes and numeric measurements.
