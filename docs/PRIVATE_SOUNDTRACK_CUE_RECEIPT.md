# Private soundtrack cue receipt

`import_reviewed_soundtrack_cue_bindings.py` writes a private
`reviewed-soundtrack-cue-bindings.json` receipt after independent, repeatable
source-free comparison evidence has been reviewed. The repository ships no
receipt and no mapping.

`ReviewedSoundtrackCueReceipt::load_local()` is the native parsing boundary. It
accepts only a bounded regular non-symlink file with the exact v1 schema. It
retains only an opaque cue token, album ordinal, codec identity, and lowercase
SHA-256 digest. It rejects paths, names, text, timing, samples, duplicate
tokens, malformed digests, unknown fields, trailing bytes, and symlinks.

The receipt loader is inert. It does not enumerate audio files, construct a
catalog, decode audio, select a cue, or start playback. A future playback
integration must pass its bindings through `SoundtrackCueResolver`, which
rechecks the selected optional file's SHA-256 immediately before use and falls
back to original game audio on every failure.
