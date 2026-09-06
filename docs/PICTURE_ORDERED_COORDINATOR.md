# Ordered drawing coordinator

`PictureOrderedCoordinator` runs the ordinary draw operations for an already
captured renderer-state snapshot. `RendererFramePass::run_and_draw` exposes that
same snapshot after state frame work, state maintenance and backend maintenance.
Registration changes during callbacks do not change the captured sequence.
Duplicate snapshot slots must resolve to the same live state object; they are
not deduplicated or copied into independent cursors.

## Preparation and rounds

For each state, clear its selected-record list, invoke the required preselection
service, mark every selected record's current slot with view mask `0x78000000`,
then set the cursor to zero. Selection preserves order and duplicates. The
bounded visitor accepts at most 8192 records and rejects calls after the service
returns. Slot resolution must identify the record that actually owns the slot.

At least one round runs, even for an empty snapshot. Each round calls every
state's draw operation in snapshot order, including exhausted states. Every
ordinary `PictureOrderedDrawLoop` invocation therefore still performs its reset.
Any true return requests another complete round; a barrier never causes one
state to be drained before visiting the next state.

After round zero, before deciding whether to continue, read the live special
enable gate. If enabled, resolve the current scene context; if present, invoke
the required first-round service. These values are read after ordinary drawing,
not captured before it. The branch can run even with no ordinary work.

## Required special service

The typed service boundary retains work not yet implemented by this coordinator:

1. Test both current profiling scalars and conditionally begin profiling.
2. Produce and finalize the special stream from actual records and scene context.
3. Begin special rendering, consume produced groups/items in order, then end it.
   Empty output still requires production and begin/end calls.
4. Re-read the profiling scalars and conditionally end profiling independently.

[PicturePreselection](PICTURE_PRESELECTION.md) supplies the eligibility traversal
over a live owner-context registry. Its activation, geometry and query services
still need the actual runtime host. Neither eligibility nor special-stream
production is replaced by an assumed empty result.

## Restoration and failures

After all rounds finish, each state's cursor becomes null/inactive. Its retained
selected list is then walked in order. For each current slot, clear only bits
`0x78000000`, resolve the current associated view's order (zero for no view), and
OR `uint32(order) << 27` into the key. Bit 31 and all other current key bits are
preserved by the clearing step. This is not saved-key rollback and does not sort.

Callbacks preserve state membership, entry storage, record ownership and the
selected lists after preparation. Keys and view associations may change between
operations; lookups use their current values. Missing owner context is an error,
not equivalent to a present owner with no associated view.

A positive caller-supplied round limit is native safety policy. If work remains
at that limit, execution aborts without pretending it finished. This bounded
domain cannot wrap back into another round-zero special branch. Callback errors
or invalid live mappings poison the coordinator, preserving the executed prefix;
no final restoration is invented on failure. Discard/rebuild failed frame state
before using a fresh coordinator. Reentry, missing hooks, null state pointers
and an invalid round limit reject before effects.

All rounds belong inside one `RendererFrame` traversal. They observe the same
engine frame word; only renderer completion advances that word. These components
do not yet constitute camera admission, special rendering or a picture GPU
executor for normal startup.

## Verification

All 63 local CTest executables pass without skips. Coordinator tests cover real
ordered-loop barriers, exhausted-state resets, duplicates, live gates, current
order restoration, slot ownership, selection capacity, escaped visitors and
callback-failure prefixes. Targeted GCC and ASan/UBSan runs pass; the extended
renderer snapshot tests also pass under ASan/UBSan.

The private owned-data probe joins renderer completion, the retained snapshot,
this coordinator, ordinary reset, ordered dispatch and the source-backed camera
for all 26 prepared intro picture groups. Its single-view partition, empty
selection registry, disabled special gate, backend availability and starting
frame word are explicit test conditions, not measured startup facts. Original
assets and expected retail vectors remain outside the repository.
