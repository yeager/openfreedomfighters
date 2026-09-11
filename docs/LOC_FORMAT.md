# LOC localization boundary

`LOC` is the per-scene localization resource. It is present in 88 verified
scene archives. OpenFreedomFighters can privately extract display-text records
after installation verification, but does not publish, translate, display, or
look up retail `LOC` text yet.

## What is established

The supported Steam-English corpus uses a bounded hierarchy. The root has no
key. Every child starts with a nonempty UTF-8, NUL-terminated key segment. Its
body is a sequence of records: a zero marker has no text; a one marker owns one
UTF-8, NUL-terminated display-text value; and a larger marker normally owns a
child list. The list stores marker-minus-one strictly increasing little-endian
32-bit cumulative child boundaries relative to the bytes after that table; its
last child ends at the enclosing node boundary. One observed terminal form uses
a larger marker followed by exactly marker-minus-one sequential UTF-8 values
instead of an offset table. One marker-only terminal has no text. Anything else
is rejected.

All 88 supported members pass this grammar. Extraction walks normalized logical
member IDs in order, then child lists depth-first, then each node's sequential
values. It assigns opaque ordinals from that order. The source-set identity is
a text-free digest of the parser revision and those ordered member IDs.

`off::data::LocStringIndex` remains a deliberately limited inventory of
bounded printable byte runs. It preserves non-ASCII bytes unchanged and makes
no key, language, source-text, or translation claim. The optional owned-data
test exercises this boundary without retaining any retail bytes in the source
tree. It does not participate in extraction.

`--probe-localization` reports a second, anonymous structural profile for the
owned startup LOC member: member size, candidate count, identifier-like count,
aggregate candidate bytes, maximum candidate length, and a digest of candidate
offsets, lengths, and identifier classification. The digest excludes all source
bytes. It enables reproducible format research across owner installations
without publishing retail text, paths, or text fingerprints. The new
`loc_catalog` decoder runs only after full installation verification and only
on a private-cache miss. Its public tests use project-authored synthetic strings
exclusively. A cache hit computes the source set from member names only and
does not decode retail text.

## Required recovery before retail localization

1. Recover native key lookup: key-path shape, case behavior, duplicate
   precedence, missing-key behavior, and locale selection.
2. Recover formatting, placeholders, plurals, and voice/subtitle linkage.
3. Compare controlled Windows observations with the native reader only as
   private clean-room evidence. Do not publish captured strings or archives.
4. Add source-free tests for every recovered lookup rule before connecting a
   runtime localization API.
5. Only then introduce project translations for the 20 supported UI locales;
   retail strings remain user-owned data and must never enter the repository.

Until these gates are complete, F10 and startup messages use the project's own
20-locale catalog. This is intentionally separate from game-text localization.
