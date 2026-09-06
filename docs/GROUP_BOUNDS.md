# Group bounds recomputation

`graphics::GroupBoundsRecompute` implements the reviewed child-union callback for
the intro language group and window root. It consumes the actual ordered runtime
child list, live owner metadata and an explicit eligible parent-space bounds
getter. It does not recursively initialize children or substitute authored values
for current runtime bounds.

## Ordered traversal and two unions

For each child, runtime `0x40000` skips all further operations. Otherwise, query
an existing owner's opt-out, then its name, then the bounds getter. Missing owners
skip the first two queries and use the original fallback name. The exact
case-sensitive name `MOVETOCREATION` excludes a child. A failed bounds getter
contributes nothing; successful results alone are validated and unioned. Hide
flags `0x400` and `0x800` are not extra filtering conditions.

Resource and child-only unions retain finite sentinels and strict comparisons.
When there is no parent owner and owner status mask `0x1` is set, only the resource
union receives the recovered root fallback. It is not an unconditional scene
default. Emptiness is determined by the original upper-X sentinel comparison,
not by a generic visited-child counter.

All child traversal completes before group outputs are written. Resource bounds
use minimum-clamped extents but radius from raw extents; an empty resource union
has zero radius. Owner fields are computed separately from the child-only union,
exclude the root fallback and retain unclamped extents. An empty owner union
writes extent zeros before center zeros; a nonempty one writes center first.
Recomputation can contract old bounds. Incremental parent propagation is a
different operation and must not reuse this behavior indiscriminately.

## Native boundary and actual-data verification

The native boundary requires stable live children/owners, non-reentrant callbacks,
nearest rounding and finite successful getter values with nonnegative extents.
The latter is a declared native restriction, not original validation. Pointer and
hook checks occur before calls. Arithmetic is validated before group writes;
unexpected callback or arithmetic failure poisons the object while retaining any
completed getter effects. No original transactional or retry guarantee is implied.

The private owned-data probe uses the legal picture's real PRM bounds, its checked
source name and the real immediate parent's complete one-child membership. It
combines those with the existing Center output under explicit retained-hierarchy,
identity-transform and eligible-getter conditions. Prior output storage is replaced,
not assumed to be the group's actual initialized bounds.

The window root also contains other children; it is not verified by recomputing
from only the legal picture. Child readiness, actual getter/suppression state,
and actual callback scheduling/admission remain integration requirements.

All 50 local CTest executables pass after this integration, together with targeted
group and zero-ID query ASan/UBSan and GCC tests and the private owned-resource probe.

## Complete ordinary intro-root membership

`intro_bounds_owner_opt_out` supplies the independently reviewed responses for
the exact ordinary picture, language-group, window-root and window-camera owner
classes. Unknown classes return no decision, not an assumed false response.
Live owner identity must be established separately; runtime resource flags are
not class metadata.

The ordinary intro camera opts out unconditionally. After the resource-flag guard,
this prevents both name lookup and bounds retrieval. It does not disable camera
rendering or imply zero camera bounds. The root's language group and fade picture
still contribute their actual eligible geometry.

The private probe now follows the root's complete authored child membership under
explicit retained-runtime-identity conditions. It materializes the fade picture's
real PRM bounds with an admitted sizing callback, recomputes the language group,
then recomputes the root from those children while verifying that the camera getter
is never called. An explicit subsequent Center operation passes through the shared
deferred position service and updates both ancestors without contracting existing
sibling bounds. The root's status, dimensions, suppression and callback admission
are supplied test conditions, not a captured original startup or resize sequence.

The complete-root probe and all 51 local CTest executables pass, along with the
updated owner-query/group tests under ASan/UBSan and GCC.
