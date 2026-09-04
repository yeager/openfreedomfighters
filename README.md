# OpenFreedomFighters

OpenFreedomFighters is a clean-room, native reimplementation and static-recompilation research project for the Windows version of *Freedom Fighters* (2003). The target platforms are macOS, Linux, and Steam Deck. A legally purchased PC installation is required at runtime; this repository contains no game assets, original executable code, or encryption keys.

The project has two runtime profiles:

- **Original** preserves gameplay, timing, presentation, and content while adding a portable renderer, modern input, stable widescreen output, and localization infrastructure for 20 languages including Swedish.
- **Modern** uses the same gameplay simulation and user-supplied data, with modern rendering, scalable resolution, improved lighting and shadows, higher-quality filtering, accessibility options, and unlocked presentation frame rates where simulation safety permits.

This is an independent fan project and is not affiliated with or endorsed by IO Interactive, Electronic Arts, or the relevant rights holders. *Freedom Fighters* and related names and assets belong to their respective owners.

## Status

Phase 0, evidence mapping and architecture. The Steam 2020 Windows build has been inventoried locally. No playable build exists yet.

## Repository rules

Do not upload game files, extracted assets, decompiled source, disassembly listings, original strings/dialogue, or generated files that substantially reproduce the original program. See [CLEAN_ROOM.md](CLEAN_ROOM.md) and [DATA_POLICY.md](DATA_POLICY.md).

## Inspect your installation

The inspector records hashes and structural metadata without extracting or copying assets:

```sh
python3 tools/inspect_install.py /path/to/FreedomFighters
```

Use `--private-paths` only for a local report that will not be committed.

Generate an aggregate resource-format census (at most 32 bytes read from each archive member):

```sh
python3 tools/resource_census.py /path/to/FreedomFighters
```

## Documentation

- [Current technical map](docs/TECHNICAL_MAP.md)
- [Resource-format census](docs/FORMAT_CENSUS.md)
- [Windows ABI replacement map](docs/ABI_MAP.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Roadmap and acceptance gates](docs/ROADMAP.md)
- [Localization plan](docs/LOCALIZATION.md)
- [Clean-room protocol](CLEAN_ROOM.md)
