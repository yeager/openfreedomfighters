# Intro renderer resource relations

`IntroRendererResourceRelations` implements initial loading and queries for the
supported intro's renderer-container relation lists. `IntroRuntime` owns the
container and resolves members through its canonical source-to-resource mapping.
The container retains copied payload bytes and ordered resource memberships.
It does not create views, accepted draw records, or GPU resources.

## Selectors and framing

The directory's unsigned auxiliary value becomes the live resource's relation
selector. A nonzero selector is a **word offset** to a parsed prefix group's
head, not a group ordinal, byte offset, owner handle, or draw identifier.
Resources may share a selector and therefore the same list. Queries read the
resource's current selector; there is no second manually populated registry.

Selector zero is unassigned and returns `nullopt`. An assigned group containing
only a null terminal reference is an empty list and returns an empty vector.
Header words, reference words, workspace offsets and out-of-range values cannot
serve as selectors. Loading validates every current resource's selector.

The existing [GMS framing](GMS_FORMAT.md) bounds the four-word header, prefix,
and two workspace ranges. Each supported group starts with a zero dynamic-chain
head and has at least one tagged reference. Bit 0 ends the group; bits 2–0
are excluded from its source address. Nonzero initial chain heads are rejected.

## Member decoding

Each nonzero address resolves through `(address + 0x60) | 0x40000000` to a live
canonical resource. Source addresses, relocated address proxies, native resource
identities and owner handles remain separate domains. In particular, an original
null reference must not be inferred from a relocated proxy equal to zero.

Bit 1 denotes a range ending at the following reference's address. The reader
emits the start and successive 112-byte source slots strictly below that
endpoint. Normal processing of the following word then emits the endpoint,
including its own tag behavior. Every intermediate slot is resolved separately;
native handles are never incremented. Endpoints must be nonzero, increasing,
within the source domain and separated by a multiple of 112. Source-slot
alignment is also checked by the canonical resolver. Bit 2 has no query effect.

Queries preserve authored order and duplicates. The supported null form is a
single null terminal reference. Mixed-null and ranged-null groups are rejected,
not silently filtered. These are explicit native admission restrictions; the
original query's broader first-null behavior is not implemented here.

The native reader caps expanded membership at one million entries per container
and publishes all groups only after validation succeeds. Failed reads publish
no partial relation lists. Queries return owned vectors rather than the
original reusable scratch view. These bounds and failure/output policies are
native safety choices, not claims about original allocation limits.

## Loader ownership and saved state

The ordinary tail selects an allocation diagnostic state and saves the returned
32-bit previous-state token. It then constructs the scene-owned container,
reads the relations and validates live selectors, before restoring the same
service using that saved token. The restoration is **not** an AddRef/Release
operation. The former construction-reference-release interpretation was wrong.

The container owns its copied payload, workspace and relation vectors. Members
are borrowed canonical resources; reading or destroying the container neither
retains nor destroys those resources. Queries reject missing resources and
failed scene state. The scene owns the container until teardown.

On a native construction, read, selector or restoration failure, the partial
container is discarded. Cleanup attempts restoration if it has not already
been attempted, without retrying a failed restore call. Exception restoration
is a native safety policy; the reviewed original sequence establishes normal
successful return only. The tail rejects reentry and copies its callback table
so selection cannot replace the paired restoration callback during the call.
Source views still require valid caller-owned storage until copied.

## Remaining work

Dynamic append, removal, reciprocal updates and dirty-flag propagation are not
implemented by this initial-load/query class. Its immutable authored lists must
not stand in for those mutations. The outer-tail two-word List associations use
the separate `+0x70` reference conversion and still need their own concrete
service. Neither relation family produces an accepted generic picture record.
The remaining drawing boundary is described in
[Intro renderer association](INTRO_RENDERER_ASSOCIATION.md).

## Verification

The 2026-09-11 Linux x86-64 suite passed all 127 CTest tests with software Vulkan.
All 14 targeted controller, prepared-resource and relation tests also passed
ASan/UBSan with leak detection.
Independent relation and loader fixtures cover typed members, ranges, shared
selectors, malformed data, stale queries, token restoration and failure paths.

A separate private run constructed all 470 resources from the verified owned
intro and ran the ordinary reader bracket with its current partial reader
coverage. The concrete relation reader used that runtime's canonical mapping:
157 resources had assigned selectors, 313 were unassigned, 97 assigned groups
were empty, and expansion produced 248 members. The run also validated the
MovieControl reader/service binding without invoking any bound service.
It did not run the complete loader tail, global activation, or intro playback.
