# ParamAnim event references

This contract corrects the optional event field in the existing ParamAnim
reader. It supersedes the previous `mode_word` interpretation and its numeric
default. It does not implement event dispatch or animation evaluation.

## Detached parsing

The optional compact class-10 field is an authored event reference. Preserve
its full unsigned 32-bit payload and presence separately from a resolved event
ID. Absence means no event update. Explicit zero means an update to event zero.
Do not truncate the authored reference before resolution.

The two boolean controls have independent false/true defaults. They do not
define an event default of `0x0100`. The reader leaves the existing event ID
unchanged when the optional field is absent. A newly allocated replacement
component may explicitly start with event ID zero; that is replacement state
initialization, not evidence of an original constructor assignment.

Retain all other numeric conversion, field ordering, optional scalar, and
bounded consumption rules of the existing parser.

## Runtime resolution

Use the actual `IntroRuntime::source_event_name_mapping()` populated before
ROOT construction. Zero resolves to zero without accessing a mapping entry.
A nonzero authored reference indexes that table directly. Check the complete
32-bit reference against the table size and require an engaged entry. Only
after resolution retain the low 16 bits of the mapped event ID.

An absent field preserves the component's existing event ID; do not overwrite
it with a parser default. Keep unresolved references and resolved component
state distinguishable in the API. Remove misleading `mode_word` naming.

Resolve before publishing owner flags, component state, or reader admission.
A missing or out-of-range entry rejects the supported reader without partial
publication. Bounds checking is the replacement's admission policy. Do not
substitute the authored index or zero for a failed lookup.

## Synthetic validation

Cover absent and explicit-zero fields separately, including preservation of a
supplied nonzero prior event ID. Test independent boolean defaults and fresh
replacement zero initialization. Use a synthetic mapping where authored index
and event ID differ. Check high-word authored references before truncation,
low-word truncation after successful mapping, missing entries, out-of-range
references, and atomic failure of the production reader. Exercise the actual
scene registry provider in a runtime fixture rather than testing only an
isolated resolver. Preserve existing framing and scalar-conversion coverage.
