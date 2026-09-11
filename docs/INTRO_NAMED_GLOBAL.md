# Intro named-reference properties

This behavior-only contract was recovered from the supported owned executable
and reviewed separately from implementation. No retail property names or data
are included here.

## Supported behavior

The intro's named/global section contains a comma-separated list of two names
and two scalar null references. Names are used in source order, without trimming.
Compact class 8 selects registry type `0x10`: its value is a four-byte reference,
not an ordinary signed integer. A present null entry is distinct from an absent
property.

The named reader, Window property registration and scene camera-property lookup
use the same scene-owned registry. The registry copies names and scalar values.
Full-name matching is case-insensitive for ASCII; inserting an existing name
replaces its prior entry. Different-length names do not match. No referenced
object is acquired for a null scalar.

The loader relocates the complete tagged block first, resets the reader to its
base, then invokes the typed reader. In this null-only form, relocation validates
the references but performs no lookup or byte replacement. Both entries are
stored before renderer-resource processing. Releasing the loader's temporary
block must not remove them or invalidate their names.

The registry belongs to the scene, not the application-global preferences store.
It is cleared on scene reset and released at destruction. Native scene teardown
already owns this lifetime; loading the named block must not clear properties
created earlier by Window or other owner construction.

## Native admission limits

The original reader supports more forms than this implementation. The supported
intro path is deliberately restricted to:

- Two nonempty printable-ASCII names, at most 1,024 bytes each, with one comma
  separator and no names equal under ASCII case folding.
- A complete 16-byte tagged block with a zero upper header byte.
- Two raw `0x08` scalar tags, each followed by a zero four-byte reference.
- The final delimiter and terminator, checked independently of typed reading.

The original dispatch uses the low six tag bits, and its block initialization
uses the low 24 header bits. Rejecting other tag/header forms is a native
supported-data restriction, not an assertion that the original rejects them.
Nonzero relocation, continuation groups, other scalar types, strings, nested
data and non-ASCII comparison semantics require separate contracts.

The name-driven original reader returns after its second value, before the
delimiter and terminator. Native whole-block validation does not mean those
suffix bytes were consumed by the original typed reader. Section padding stays
outside the bounded block.

Validate both names and values before changing the scene registry. Staging
updates and preserving the registry on validation/allocation failure is an
explicit portable safety policy; the original has no reliable transactional
failure result. Errors must not include the private names or payload values.

## Verification scope

Independent fixtures cover copied names, present-null lookup, full-length
ASCII-case replacement, whitespace preservation, limits, malformed second
values and framing rejection. Runtime integration tests additionally prove that
the real scene registry sees the entries in the loader tail, that earlier
unrelated properties survive, and that renderer processing occurs afterward.

The full 125-test Linux x86-64 suite and all 13 targeted reader/resource/fade
tests under ASan/UBSan with leak detection pass with these changes. A separate owned-data
check validates the supported two-entry null form, registry type 16 and zero
reference lookups. It does not invoke scene activation or export names/values.

Owned-data verification reports only aggregate counts and outcomes. This reader
does not materialize the renderer container or complete scene initialization;
those remain I1 work in the [intro and menu plan](INTRO_AND_MENU_PLAN.md).
