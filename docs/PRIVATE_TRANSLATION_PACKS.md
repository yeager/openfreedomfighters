# Private translation packs

`PrivateTranslationPack` loads an optional translation file only from a direct,
regular, non-symlink child of a caller-selected local directory. The file is a
versioned binary record containing parser identity, text-free source-set
identity, canonical locale tag, exact ordinal span, a complete flag, and pairs
of opaque IDs with UTF-8 translated text. It has no source-text or English-text
field, and the loader emits no pack contents to logs.

Before loading, `translation_source_binding` accepts only text-free extraction
metadata: parser identity, source set, and its contiguous ordinal span. The
retail-text snapshot remains private to the cache implementation. The loader
requires an exact match. IDs must be canonical
`off.retail.<source-set>.<ordinal>` values inside that span; duplicate IDs,
invalid UTF-8, unsupported locale tags, malformed files, and incomplete packs
declared complete are rejected.

`PrivateTranslationResolver` chooses an explicit locale, then the system locale
preference list, then English. A partial preferred pack may fall through for an
untranslated ID. This boundary neither performs retail key lookup nor connects
to UI rendering. A text-free ID does not determine copyright status: translated
retail text remains local unless its provenance and licence permit publication.

## Enrollment

After a verified installation has produced private, text-free cache metadata,
OpenFreedomFighters obtains SDL's per-user preferences location and probes only
these direct filenames below `translation-packs/`:

`en`, `sv`, `da`, `nb`, `fi`, `de`, `fr`, `es`, `it`, `pt-BR`, `pl`, `cs`,
`hu`, `ro`, `tr`, `ru`, `uk`, `ja`, `ko`, and `zh-Hans` (each with the
`.offl10n` suffix). It does not enumerate or recursively scan the directory.
Missing, malformed, mismatched, symlinked, or wrongly named files are ignored
without affecting startup. The declared pack locale must equal its canonical
filename.

Enrollment builds a private resolver after the cache and source binding have
been validated. For an already-approved opaque ID, a matching local translation
has precedence and a missing translation falls back to the matching cached
English record. It still has no UI key lookup path, so it cannot alter retail
presentation until that native lookup contract is independently recovered.

## Reviewed retail lookup artifact

An independently reviewed native lookup contract may be placed locally as the
single binary file `reviewed-retail-lookup.offlookup` below the application
preferences directory `reviewed-retail-lookup/`. The loader never scans the
directory and accepts only a direct regular, non-symlink file. The format is
versioned and bounded; it carries parser identity, source-set identity, exact
ordinal span, and opaque reviewed-site-to-ordinal observations. It contains no
retail text, key, component name, address, or game path.

The loader validates the complete source binding against the private enrollment
metadata before constructing an artifact. Invalid, absent, symlinked, oversized,
trailing, malformed, or mismatched records produce no bindings and do not affect
startup. A successfully loaded artifact is admitted only by the existing
source-span and duplicate-site checks, then a UI caller can resolve an observed
opaque site through the private catalog or an optional local translation pack.
