# Intro audio cue inventory

`--probe-intro-audio` validates the complete source-backed relation from every
prepared intro sound owner to its SND definition and exact WHD stream row. Its
output is aggregate-only: owner, distinct-definition, distinct-stream, and
local/global-bank counts. It does not print logical identifiers, paths,
resource offsets, samples, durations, hashes, or retail text.

The probe neither decodes nor reads encoded audio, opens an output device,
creates a channel, sends an acknowledgement, or starts playback. It is an
evidence boundary for future intro sound work, not a playable intro-audio path.

An SND/WHD relation is not an in-game music cue. Optional Steam soundtrack
FLAC/MP3 files remain unavailable to intro playback until a separate private,
repeatable cue comparison produces an opaque reviewed cue binding. Album order,
titles, codec, stream metadata, or duration cannot create that binding. Until
then, the only valid eventual source for every intro owner is its original game
audio.
