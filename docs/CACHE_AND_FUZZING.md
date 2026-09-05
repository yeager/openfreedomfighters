# Derived cache and parser fuzzing specification

This document defines the remaining Phase 1 cache and fuzzing work. It is a
delivery contract, not an implementation claim. The current runtime reads the
verified installation directly and public tests use independently authored
fixtures; there is no derived-asset cache or dedicated fuzz-target suite yet.

## Security and data boundary

Retail files and values derived from them remain user-owned local data. A cache
may improve local loading time, but it does not make cached bytes redistributable.
The repository, CI, release packages, seed corpora, crash artifacts, and logs must
never contain retail payloads, extracted strings, identifiers unnecessary for
interoperability, screenshots, decoded media, or private analysis.

Synthetic fixtures are required for public parser tests and fuzz seeds. Private
compatibility runs may consume a verified installation in place, but retain only
aggregate pass/fail and bounded structural measurements suitable for publication.
A fuzzer failure found with retail input must be reduced to an independently
authored reproducer before entering Git history.

## Cache identity and layout

The cache is disposable and never authoritative. Each entry key covers:

- a cache-format UUID and monotonically revised schema version;
- the engine build/decoder compatibility version;
- the supported retail manifest identity and normalized virtual source path;
- source size and a cryptographic digest of the exact source bytes;
- parser or decoder options that can change the portable result; and
- the portable-model version, with a target/backend key only for genuinely
  platform-specific products such as compiled shaders.

Entries use an explicitly little-endian, fixed-width envelope with magic,
version, kind, header size, payload size, source identity, payload digest, and
declared resource counts. Every offset, count, multiplication, decompressed size,
and allocation is checked before use. Portable models never serialize native
pointers, structure padding, container capacity, ABI-dependent enums, or host
paths.

The cache root is outside the installation and repository. Entries are written
to a unique sibling staging file, flushed, validated through the normal reader,
and atomically renamed. Interrupted writes, unknown versions, failed digests,
impossible counts, and trailing bytes cause a cache miss and recoverable removal,
never partial model use. Concurrent readers see a complete old or new entry;
writers coordinate per key. A bounded size policy evicts least-recently-used
entries without touching retail data, settings, or saves.

Diagnostics expose only kind, version, byte counts, hit/miss reason, and a short
non-reversible local correlation token. They do not print source paths, retail
identifiers, full hashes, payload bytes, or decoded content. “Clear cache” must
resolve and validate the application-owned cache root before a recoverable,
narrowly scoped deletion.

Any mismatch in source digest, manifest, schema, model version, relevant decoder
option, or backend product version is a miss. Readers never interpret a newer
schema best-effort. A bounded, tested migration is allowed only when safer than
rebuilding; otherwise the entry is regenerated from verified source data.

Original and Modern may share source-derived portable models. Presentation-only
products include their profile and effective quality inputs in the key. Cached
artifacts cannot influence simulation, input timing, RNG, AI, collision, saves,
or checkpoint hashes.

## Fuzz-target matrix

Every public entry point accepting untrusted bytes needs a dedicated target, not
only a unit test that reaches it indirectly.

| Family | Required target boundary |
|---|---|
| Containers and VFS | ZIP records, archive overlays, bounded views, packed resources, and ZGF bundles |
| Scene metadata | GMS images, scene-support data, render maps, and packed spatial trees |
| Render assets | texture catalogs/decoders, primitive catalogs/topology, and picture resources |
| Audio | WHD metadata and PCM/IMA ADPCM/Vorbis dispatch with bounded output |
| Cache | envelope/model readers and invalidation decisions |

Each target accepts one byte span, performs no filesystem or network access,
uses deterministic limits, and treats a clean rejection as success. Stateful
formats use a compact synthetic wrapper only when needed to partition input into
bounded sources. Targets do not weaken production validation or add fuzzer-only
parser behavior.

Seed corpora contain the smallest independently authored valid member for each
supported variant and focused invalid boundary cases. Dictionaries contain only
public structural constants necessary for parsing. Mutation coverage includes
truncation at every field boundary, integer overflow, overlapping or unordered
ranges, duplicate identities, hostile counts, decompression expansion, invalid
indexes, malformed paths/text, and trailing-data policy.

## Execution, regressions, and acceptance

The primary configuration uses Clang libFuzzer with ASan and UBSan. A bounded
deterministic smoke budget runs in pull-request CI; scheduled and local jobs run
longer campaigns with the same checked-in synthetic corpus. Per-target RSS,
input-size, timeout, and decoded-output limits prevent suite starvation.

A finding is complete only after reproduction, minimization, classification, a
fix at the narrowest shared boundary, and a synthetic regression. Crash inputs
and logs pass Gitleaks and the retail-content audit before upload; public
automation never uploads an unreviewed fuzzer artifact.

The cache gate requires cold/warm equivalence for every model, invalidation and
interrupted/concurrent-write tests, safe fallback on all corruption, and a clean
content audit. The fuzz gate requires every production byte-parser in the target
inventory, seeds reaching every supported variant, prescribed sanitizer campaigns
without findings, and regressions for historical findings. Private sanitizer
parsing of startup and first-level retail assets is additional compatibility
evidence and does not replace public malformed-input coverage.
