# Position update service

`graphics::PositionUpdateService` implements the independently reviewed scheduling
boundary behind the picture position setter. It is shared by a scene manager,
not recreated for each picture. `CenterPicturePosition` can notify it using the
same live resource flag word. It is not a complete scene manager or a replacement
for the downstream spatial, bounds and maintenance operations.

## Immediate and deferred notifications

Mode, collection enablement and signed suppression are explicit per-operation
inputs. Immediate mode calls the optional spatial-change hook and then bounds
propagation with no incoming child. It does not inspect or flush the queue and
does not use the deferred collection or resource-flag gates.

Deferred mode returns when collection is disabled or resource flags intersect
`0x20200000`. Otherwise an exactly full, 50-entry queue is flushed first. The
original admission checks are not repeated after that flush. The resource flags
are ORed with `0x23000000`, then its opaque manager handle is obtained and appended.
Zero handles are retained too; resolution decides whether they represent a live
resource. The service does not invent a universal generation or pointer format.

## Flush order

1. Resolve every queued handle once in order. Missing resources are dropped.
   Clear only `0x20000000` on each survivor before retaining its live identity.
   The original queue count remains visible throughout this first pass.
2. If collection is disabled, clear the count and return. Otherwise publish the
   survivor count before processing them in their preserved order.
3. Read each survivor's current flags. `0x200000` skips per-resource work;
   otherwise `0x40000` selects maintenance; otherwise bounds propagation runs
   only when signed suppression is nonpositive.
4. Invoke final-batch processing with all survivors, including those skipped in
   step three. Invoke it for empty batches too. Clear the count only after it
   returns. The retained `0x03000000` bits are not silently removed.

Operation hooks may make their declared resource-flag changes. Later predicates
read those changed flags, not a stale snapshot. Original loops observe live queue
counts; the native boundary prohibits queue mutation and reentry, making its
bounded traversal equivalent without claiming original snapshot semantics.

## Safety and remaining integration

The caller owns resources and keeps identities, registry and modes stable during
an operation. Required hooks are validated before effects; spatial notification
is optional. Hooks must not throw on admitted paths. An unexpected exception
retains completed effects and poisons the service, which rejects subsequent
operations. This explicit native failure policy avoids claiming an original
rollback or valid retry after partial resolution and compaction.

The default intro filename selects deferred rather than immediate mode in the
reviewed scene constructor. Ordinary loading disables collection early, then
clears and enables it before the global component initializer. Global phase one
finishes before phase two, both traversing reverse component-construction order.
This establishes the queue boundary around those passes, not each component's
runtime eligibility or the effects of intervening callbacks.

With explicit entry suppression `S` and no callback counter mutations, ordinary
loading increments before source construction, decrements before the initial
root-bounds refresh and global initialization, then increments again before
return. Thus initialization sees `S`, while successful load returns with `S+1`.
Collection enablement does not justify a zero suppression input at the first
update. Context suspension saves the old counter and resets the active counter;
restoration copies it back, and cleanup has a decrement. Their actual call
placement must be preserved rather than inventing an extra balancing operation.
The root refresh requires exactly zero, unlike the service's nonpositive guard.
Any future native counter implementation must define 32-bit wrapping or explicitly
reject overflow; mathematical `S+1` is not permission for C++ signed overflow.

The private owned-data probe connects the real legal picture's authored position
through Center to this queue and checks its deferred processing order. Supplied
engine dimensions, mode and native identity are test conditions. Bounds and batch
hooks remain visible integration boundaries; counting their calls is not proof
of complete scene propagation or a rendered intro frame.

All 47 local CTest executables pass after this integration, along with targeted
service ASan/UBSan and GCC tests and the private owned-resource probe.

## Completed final batch for plain pictures

`complete_plain_picture_position_batch` closes the final handler's scene behavior
when every surviving live owner is the concrete plain-picture class. It validates
the entire batch through a pure live owner-class lookup, including hidden entries
as a stronger native admission policy. Null resources, unknown owners and mixed
classes reject rather than being silently discarded. Broader batches still need
the explicit general handler.

For this admitted family neither of the original selected class masks is present.
The selected list is empty, and the concrete final handler performs no scene
mutation with or without a spatial service. Runtime position-dirty flags must not
be mistaken for owner-class bits. This conclusion does not remove preceding
bounds/maintenance processing or imply that an arbitrary queue contains only
pictures. The service still invokes the final callback before clearing its count.

The private legal-picture probe now uses this checked handler, with owner identity
and class tied to the previously verified concrete picture source. Bounds work
remains an explicit unresolved operation; final-batch completion alone is not a
complete scene update or proof of visibility.
The updated handler and queue integration pass all 47 local CTest executables,
targeted ASan/UBSan and GCC tests, and the private owned-resource probe.
