# MovieControl phase-one recovery boundary

Status: unsupported. The retained owner and component readers prove one
MovieControl identity and make the phase-two callback eligible for a future
global pass. They do not prove that phase one is empty, nor do they identify a
phase-one callback.

## What is established

- The controller has one source-bound owner, one source-bound component, and
  the checked reader receipts described in
  [COMPONENT_LIFECYCLE.md](COMPONENT_LIFECYCLE.md).
- Its later phase-two callback has a separately reviewed service contract.
- The later ordinary event path uses the fixed phase-two deadline and requires
  phase-one completion before it can prepare the first cut.

None of those facts establish the preceding lifecycle dispatch. In particular,
the component's requested mask is an admission request, not evidence that a
given callback ran or that its owner transition is safe to synthesize.

## Missing contract

The following facts are required before phase one can be implemented or added
to normal startup:

1. The exact dispatcher entry that selects the MovieControl component for
   phase one, including its order relative to other components.
2. The concrete callback identity and its required owner, reader, event and
   external-service preconditions.
3. Every observable callback effect: component and owner status changes,
   event enrollment or removal, retained-state changes, and failure behavior.
4. The completion boundary used by the later event-16 gate. A synthetic
   completion bit would make the cut path appear ready without proving that
   the original callback's work occurred.

Private static review found an update-shaped routine consistent with the
already documented fixed-delay path, but the retained artifact does not link
that routine, or any candidate phase-one routine, uniquely to the
MovieControl factory. The surrounding base implementation is shared by other
visual classes. It is therefore insufficient evidence for a public callback
contract.

## Required private observation

Use an isolated, owned installation to record a source-free lifecycle trace
that correlates the constructed controller identity with its phase-one
dispatch. The trace must report only structural metadata: dispatch order,
phase number, component/owner identity relation, pre/post status masks,
event-membership change, success/failure, and whether an external service was
entered. It must not export game strings, assets, offsets, payload bytes,
screenshots or executable material.

The implementation may proceed only when that trace identifies one callback
and all of its required effects. The resulting public tests must use authored
fixtures or source-free recordings; normal startup remains fail-closed until
then.
