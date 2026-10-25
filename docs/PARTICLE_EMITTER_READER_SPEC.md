# Ordinary geometry ParticleEmitter reader

This contract covers bounded reading and reference resolution. It does not
admit particle emission, event dispatch, component initialization, rendering,
or global scene activation.

## Owner and dispatch

Accept an ordinary geometry owner (`0x00200002`) with exactly one
`ZGEOM_ParticleEmitter` attachment. Check the actual constructed component,
owner, resource, source row and attachment slot identities. Unsupported owner
variants, Group owners and additional siblings remain outside this contract.
Do not select behavior from row numbers, block sizes or resource names.

Input is the complete loader-bounded DBLK including its four-byte total size.
Require an exact size match. The ordinary geometry owner consumes no body
fields and has no owner-reader side effect. Its component suffix begins
immediately after the size word; no owner-prefix separator is consumed.

Use the bounded component dispatcher. Require one complete component boundary
and an exact terminal continuation without extra bytes. The component reader
receives a delimiter-free slice and returns a suffix into that same slice.
Unread fields remain for the dispatcher, not for fabricated emitter state.

## Detached component fields

Read these fields in order, using neutral names where meaning is unproven:

1. Template reference: integer conversion, retaining all 32 bits and tag class.
2. Boolean A and Boolean B: integer conversion compared with zero.
3. Scalar A and Scalar B: float conversion.
4. Vector A: exactly three continuation-group float values.
5. Lower and upper angles: float conversion, then binary32 multiplication by
   the binary32 value of pi/180. Do not clamp, reorder or normalize angles.
6. Vector B: exactly three continuation-group float values.
7. Scalar C: float conversion.
8. Event reference: integer conversion, retaining all 32 bits and tag class.
   Resolve its runtime identity before narrowing to 16 bits.
9. Boolean C and Boolean D: integer conversion compared with zero.
10. Scalar D: float conversion.
11. Optional template name, with boundary behavior described below.

Integer conversion of binary32/64 truncates toward zero to signed32; invalid
or overflowing values produce signed32 minimum. Integer classes 3, 8, 10 and
11 preserve their payload bits. Float conversion narrows binary64, preserves
binary32, and converts integer-class payloads as unsigned32 to binary32.
Preserve the established ParamAnim numeric-conversion behavior.

NUL-string classes 4/5 and length-delimited classes 7/9 yield numeric zero
while consuming their complete bounded encoding. Required typed fields reject
class 12, unknown classes, separators and terminals. Do not widen these rules
merely because the generic framing scanner can skip more classes. Extent
words include their own four bytes; validate every encoding within the slice.

A vector group consumes its initial value regardless of that tag's high bits.
It continues while the NEXT tag has bit `0x40`; `0x80` alone does not continue
the group, and exact terminal never continues it. Require exactly three values
per vector. Reject shorter or longer groups. This is an explicit bounded
admission policy, not a claim that the original helper checked capacity.

Name classes 4/5 yield their NUL-terminated contents. Class 7 with extent four
means no name; larger class-7 payloads require a NUL within their extent.
Other supported consuming classes yield an empty name and advance. Slice end
immediately after Scalar D means an empty name without consuming anything;
the enclosing dispatcher must still validate its delimiter and terminal.
Truncated explicit strings or length fields remain errors. Empty and absent
names do not trigger fallback lookup.

## Reference domains

Loader relocation applies to class-8 and class-10 values throughout the block,
not just to fields named as references. Detached parsing describes raw compact
conversion and must retain enough tag/payload provenance to distinguish it
from runtime interpretation. For this production profile, nonzero class-8 or
class-10 values in other consumed scalar, vector, boolean or optional-name
positions are unsupported: reject them before lookup effects or publication.
Their zero payloads are safe because relocation leaves zero unchanged. Do not
silently interpret an unrelocated nonzero reference as an ordinary numeric
value. This is an explicit supported-domain boundary, not the native reader's
complete accepted tag set. Generic unread-tail framing remains separate.

Translate nonzero class-8 template references using
`local_source_for_authored_reference`, the resulting row's constructed
`directory_resource_mapping_`, and its live resource/owner association.
These are separate checks; an authored index is not a runtime owner handle.
Zero is a lookup miss. The existing authored-reference helper handles the
high marker bit. Out-of-range authored references reject under the host's
bounded admission policy. A valid translated resource without a live owner
is a fallback-eligible lookup miss.

Other consuming template-field classes may supply zero. Nonzero values in
their unintegrated native domains reject production admission; detached
parsing may retain them. In particular, class 10 is not a directory reference.

For the event field, class 10 uses `source_event_name_mapping()`: zero remains
zero; a nonzero full-width authored index requires an engaged in-range entry.
Retain the mapped ID's low 16 bits only after lookup. Other consuming classes
may supply zero; nonzero unintegrated event domains reject. Do not truncate
an authored index before validating it or silently substitute its raw value.

## Registry provider and effect ordering

The retained template identity is a resource handle, not an owner pointer.
Keep it separate from the raw authored field and any derived resolved owner.
Numeric success retains the translated resource unchanged. Numeric miss with
no fallback name retains that unresolved resource reference unchanged. A
successful name fallback replaces it with the matched owner's resource handle;
a fallback miss replaces it with zero. The replacement collection contains
owner tokens, so convert its matched owner with `resource_handle(owner)`
before retaining the template identity. Do not store the collection member's
owner token as the component's template resource.

Resolve the numeric template first. A live result suppresses name lookup.
For a miss with a nonempty name, use the actual scene object property
`ParticleTemplates`, resolved through the application handle-collection
service. Construction already appends template owners in construction order.
Do not rebuild or sort the registry or scan arbitrary scene owners instead.

If the property is absent or its token does not resolve, create an empty
application handle collection and publish its token to that property. Iterate
its members in order, skip unresolvable owner/resource associations, and match
the retained constructed owner's name exactly and case-sensitively. The first
match wins. Do not require the template's deferred reader or emission lifecycle
merely to resolve its already-constructed identity and name.

Complete all bounded parsing, terminal checks and reference-domain validation
before a mutating registry lookup. Malformed input leaves no typed state,
admission receipt or registry mutation. A valid fallback search that misses
is still a successful reader outcome: a lazily created empty registry persists.
Publish the typed component state and reader receipt only after resolution.
No lifecycle admission follows from completing this reader.

## Synthetic validation

Test both vectors and continuation bits independently; short/long groups;
all numeric classes, unsigned float conversion, invalid float-to-int, angle
conversion, class-7 and NUL names, absent names, and unread generic tails.
Verify that detached raw numeric conversions do not imply runtime admission:
nonzero relocation classes in undesignated consumed positions reject before
registry effects, while zero payloads remain safe.
Reject malformed strings/extents, unsupported typed classes, missing/extra
boundaries, wrong owners, unsupported siblings and late-invalid blocks.

Exercise the actual runtime provider with synthetic constructed template owners
and registry order: numeric success bypasses name lookup, zero reference allows
name fallback, duplicate names choose the first live match, invalid members
are skipped, case mismatch misses, and lazy creation persists on a valid miss.
Test full-width template/event mapping, missing entries, post-map narrowing,
and failure before any component publication or registry side effect.
