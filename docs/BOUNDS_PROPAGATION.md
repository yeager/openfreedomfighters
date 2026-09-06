# Incremental identity-basis bounds propagation

`get_identity_parent_bounds` and `IdentityBoundsPropagation` connect current local
bounds and positions to expansion-only updates of shared ancestor resources.
They do not recompute children, initialize ancestors, notify the position service
or invalidate picture caches.

## Eligibility and arithmetic

The getter returns no result for positive suppression, runtime `0x40000`, zero
radius or absent parent. Owner opt-out is a separate propagation guard, not part
of this getter. Hide flags are not extra rejection predicates. Under the explicit
identity-basis subset, parent-space center is the separately rounded sum of local
bounds center and local position; extents remain unchanged. Child upper/lower
corners have their own subsequent binary32 rounding boundary.

Propagation starts with the changed resource and no incoming child. It preserves
suppression, flag and parent-presence checks before the incoming-child opt-out
and getter checks. A zero-radius parent initializes from the incoming child
without reading its old center/extents. Otherwise, current parent corners expand
only when an incoming corner strictly exceeds them. No expansion ends the entire
walk without writing that parent or inspecting farther ancestors.

A changed parent receives rounded center, raw extents, minimum-clamped stored
extents and raw-extents radius, with the reviewed runtime dirty writes. Its owner's
fields then receive clamped extents followed by center. This is deliberately
different from group recomputation's unclamped child-only union. Propagation
continues with the updated parent only if the next eligibility checks permit it.
Existing bounds from other children are retained; there is no contraction.

## Explicit native state boundary

The caller supplies coherent shared live resources, current parent links, local
bases/positions and stable suppression/owner opt-out results. Consumed values must
be finite; radius and extents must be nonnegative, and arithmetic requires nearest
rounding. Signed-zero identity components are accepted numerically. Nonidentity
transforms are not silently approximated.

Only reached state is validated. A stopped path does not demand initialized
farther ancestors, and a root's unused local basis is not inspected. Arithmetic
and owner storage are checked before each parent's mutation. Reached cycles,
distinct wrappers aliasing an already reached resource/bounds object, or
later invalid state retain any already completed ancestor updates and poison the
object. This is an explicit native failure policy, not an original rollback or
retry guarantee. No concurrent mutation, destruction or reentry is supported.

The private owned-data probe now uses the getter for the real legal picture and
its immediate language parent. After recomputing that parent's bounds from its
complete real child list, incremental propagation correctly stops on no expansion
without demanding uninitialized root bounds. Engine dimensions, retained runtime
links/transforms and getter admission remain explicit conditions; full root
initialization and actual callback scheduling are not inferred from this test.

All 51 local CTest executables pass after this addition, along with targeted
propagation ASan/UBSan and GCC tests and the private owned-resource probe.
