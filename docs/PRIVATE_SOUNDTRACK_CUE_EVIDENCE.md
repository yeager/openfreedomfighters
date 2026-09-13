# Private soundtrack cue evidence

Optional Steam soundtrack files remain optional. A verified FLAC or MP3 must
never replace an in-game cue solely because its album ordinal, title, duration,
or codec appears similar. The native runtime still uses game audio: it does not
load the private receipt described here and this workflow does not start audio.

## Private workflow

`tools/soundtrack_cue_observation_runner.py` launches an operator-supplied
observer only when `--execute` is explicit. The observer receives literal
`fresh-isolated` mode and three fresh output paths: two independent successful
comparisons and one rejected comparison. The launcher does not discover a
game, attach to a process, accept an installation path, or relay observer
output. It accepts only the three expected bounded regular files, sanitizes
them, deletes their raw forms, and writes source-free records outside the
repository.

The allowed record contains only an opaque cue token, album ordinal, format,
and categorical timing result. It rejects titles, paths, audio samples,
durations, hashes, addresses, byte sequences, screenshots, executable data,
and arbitrary fields. A review bundle is admitted only when two success traces
match exactly under one verified-installation fingerprint and a separate run
contains only distinct rejected candidates.

```sh
python3 tools/soundtrack_cue_observation_runner.py --execute \
  --observer /private/observer \
  --workspace /private/new-soundtrack-cue-observation
```

The result includes `review-bundle.json`; it is private evidence, not a shipped
mapping. Raw observer records are deleted on success, rejection, timeout, or
observer failure.

## Hash binding

`tools/import_reviewed_soundtrack_cue_bindings.py` joins that source-free
bundle to a separate private catalog manifest. The catalog manifest has no
paths or names; each edition contains only `{album_ordinal, format, sha256}`.
The importer produces a new, mode-0600 private
`reviewed-soundtrack-cue-bindings.json` receipt containing opaque cue tokens,
album ordinals, formats, and full file digests.

```sh
python3 tools/import_reviewed_soundtrack_cue_bindings.py \
  /private/new-soundtrack-cue-observation/review-bundle.json \
  /private/catalog-binding.json \
  /private/reviewed-soundtrack-cue-bindings.json
```

The importer fails closed for duplicate tokens, missing editions, malformed
digests, symlinks, existing output, mismatched evidence metadata, or any
unreviewed input. A later native audio-lifecycle integration must independently
load and validate this receipt, re-hash the selected file immediately before
use, and fall back to original game audio on every failure.
