# Installation hashes and optional soundtrack

Every startup, including `--verify-only`, checks immutable game files against
the compiled SHA-256 reference in `src/data/install_manifest_reference.inc`.
Files with the expected size are read in full. Modification times are not a
shortcut for a successful hash. Existing structural checks still run afterward.
The splash stays responsive while verification runs; reading all data can take
longer than its three-second minimum display time.

## Reference snapshot

The reference was measured from the locally owned Steam installation on
2026-09-06. It includes the user's repaired C03A archive. It is a supported local
snapshot, not an official Steam manifest or proof of an unmodified release.
Only relative paths, sizes, hashes and roles are published, not game contents.

The 226 immutable files total 1,198,025,711 bytes:

- 184 required files: `Freedom.Exe`, `streams.wav`, and 182 files under `Scenes`.
- 36 optional soundtrack files: 18 FLAC and 18 MP3.
- Six optional support files: two covers, launcher, two DLLs and Steam app ID.

The known `Freedom_Fighters_OST/FF_OST_MP3_320/Thumbs.db` thumbnail cache is
excluded. Unrelated root files, future saves and configuration are not enrolled.
Unrecognized files under `Scenes` fail verification; unrecognized soundtrack
files are reported and never automatically trusted. Startup never rewrites the
reference to accept changed files.

The installation root may resolve through a symbolic link. Links within checked
paths are not followed. Case-insensitive path collisions are rejected for the
affected files. Files must remain unchanged during the session: startup hashing
does not provide tamper-proof handles for subsequent reads.

## Optional soundtrack

Missing soundtrack files never prevent startup. Validation is per file: a
partial album, FLAC-only installation or MP3-only installation is allowed.
Corrupt, unreadable, ambiguous or unrecognized optional files are skipped and
reported without invalidating required game data. The soundtrack directory is
excluded from the game VFS, so its size does not consume the game-data mount
limit. Optional distribution support files are excluded too.

The measured FLAC files are stereo 16-bit/44.1 kHz; the MP3 files are stereo
320 kbit/s/44.1 kHz. The original global bank contains Vorbis payloads at
44.1 kHz and 22.05 kHz. FLAC avoids lossy codec degradation, but album edits,
mastering, loop boundaries and in-game cues still need comparison. File format
alone does not establish that an album track is a drop-in replacement.

Playback policy: use a verified, decodable soundtrack version only when its cue
mapping and required timing/loop behavior are known. Prefer FLAC, then a usable
MP3 version, otherwise the game's original music. Missing or unusable album
tracks must never disable original music or create a startup requirement.

**Current implementation:** startup exposes hash-verified soundtrack candidates
and reports optional failures. There is no music playback, FLAC/MP3 decoder or
verified cue mapping yet, so neither soundtrack substitution nor audible
fallback is implemented. Hash verification is not a playback suitability test.

Tests use independent fixtures for missing/partial/corrupt optional files,
same-size edits with unchanged timestamps, collisions, symlinks and cancellation.
Retail contents are not needed by CI.
