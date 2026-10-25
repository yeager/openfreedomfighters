# Audio-resource handling

Audio resources are read from the user's installed game data. The project keeps
metadata parsing, stream access, decoding, and playback as separate stages.

Readers validate bounds and supported codec inputs before exposing data to the
audio runtime. Large streams use bounded reads; decoded output and source assets
remain local to the user's installation and are not included in this repository.

The parser implementation and synthetic fixtures live in the source tree. This
document intentionally omits game-specific layouts, values, inventories, and
payload details.
