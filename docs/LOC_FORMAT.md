# LOC localization boundary

`LOC` is the per-scene localization resource. It is present in 88 of the
owned-installation scene archives, but it is not yet a decoded text catalog.
OpenFreedomFighters does not copy, translate, or display retail `LOC` text.

## What is established

Private inspection of owned `LOC` members establishes that they are structured
binary resources, not a flat sequence of strings. They contain bounded
NUL-terminated byte fields interleaved with variable-width tagged records and
little-endian reference/offset fields. The same member may contain identifiers,
displayable text, and non-text binary fields. Legacy encoding is not yet
identified; a byte span cannot be assumed to be UTF-8.

`off::data::LocStringIndex` is deliberately limited to a lossless inventory of
bounded printable byte runs. It preserves non-ASCII bytes unchanged and makes
no key, language, source-text, or translation claim. The optional owned-data
test exercises this boundary without retaining any retail bytes in the source
tree.

`--probe-localization` reports a second, anonymous structural profile for the
owned startup LOC member: member size, candidate count, identifier-like count,
aggregate candidate bytes, maximum candidate length, and a digest of candidate
offsets, lengths, and identifier classification. The digest excludes all source
bytes. It enables reproducible format research across owner installations
without publishing retail text, paths, or text fingerprints. It does not
recover a record grammar, encoding, language, or display behavior.

## Required recovery before retail localization

1. Recover the record framing and validate every offset/reference stays within
   its owning member.
2. Identify key records separately from display-text records, including
   duplicate-key and missing-text behavior.
3. Recover the original character encoding and any language-selection,
   formatting, placeholder, plural, and voice/subtitle linkage rules.
4. Compare controlled Windows observations with the native reader only as
   private clean-room evidence. Do not publish captured strings or archives.
5. Add a source-free parser fixture for every recovered record grammar before
   connecting a runtime localization catalog.
6. Only then introduce project translations for the 20 supported UI locales;
   retail strings remain user-owned data and must never enter the repository.

Until these gates are complete, F10 and startup messages use the project's own
20-locale catalog. This is intentionally separate from game-text localization.
