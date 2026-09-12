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

Enrollment currently validates packs only. It builds neither an active resolver
nor a UI lookup path, so no private pack text can alter the application until
the native LOC lookup contract is independently recovered.
