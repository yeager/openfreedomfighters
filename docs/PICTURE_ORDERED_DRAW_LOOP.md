# Ordered picture dispatch

`PictureOrderedDrawLoop` consumes a prepared ordered array, its stored cursor,
and explicit backend hooks. Each record retains a live view association and an
optional resource identity independently of its key. A view transition uses
that association, never a key nibble as an array index or camera source number.

## One invocation

The required ordinary reset hook runs before the empty-list check. Local
previous key starts at `0xffffffff`, and previous subtype starts unset.

- An equal complete key emits immediately, without any transition work.
- Otherwise subtype nine advances past the barrier and ends this invocation.
- A changed view field (`0x78000000`) either skips a reserved-field entry or
  invokes the retained view's transition hook.
- A changed subtype ends the previous subtype before beginning the new one.
- A changed resource field (`0x7ff0`) invokes binding only for a nonnull resource.
- The complete previous key is updated, even without a binding, then the record
  is emitted. Local cursor advancement follows successful emission.

At a barrier or normal end, the cursor is published before ending the active
subtype. Return value says whether entries remain. A last-entry barrier returns
false; a resumed invocation starts with a fresh reset and local comparison state.

Reserved-view skipping is conditional on a changed comparison field. An initial
reserved-field key therefore need not skip: it shares the sentinel's view field.
The unsupported complete sentinel key itself is rejected as native policy rather
than emitted with an unset subtype. Checked picture keys use subtype two and
cannot collide with that sentinel.

Null resources preserve binding; they do not unbind it. Equal resource fields
do not bind just because identities differ. Likewise identical view comparison
fields do not force a transition when associated pointers differ. These are key
comparison rules, not pointer-equality optimizations.

## Ownership and failure policy

The caller supplies already-prepared selection/rebuild results. No sorting,
cursor initialization, visibility or registration is inferred here. Entries,
identities and referenced objects must remain stable through callbacks.

Native prevalidation requires complete hooks, an in-range cursor, no remaining
sentinel key, and live associations on every remaining nonbarrier/nonreserved
entry. This association check is deliberately broader than the original's
unchecked transition dereference. Reentry rejects before effects.

Callback exceptions preserve their executed prefix. Before final publication,
the stored cursor remains unchanged despite earlier emissions; after publication,
even a throwing subtype-end hook leaves it advanced. Abort the failed frame;
this is not a transactional retry interface.

## Still needed for normal startup

The actual coordinator first prepares every matching state's cursor through a
selection hook that can mark reserved entries. It then calls every state's draw
operation once per round, including exhausted states, repeating if any returns
more work. It does not drain one state before moving to the next. First-round
special services and final cleanup are separate real operations.

This adapter does not replace those services with no-ops, implement the complete
ordinary backend reset, or admit the intro. The explicit reset hook is an
integration obligation. View hooks can use `PictureViewTransition`; resource
and emission hooks still need the real GPU executor and current material state.

## Verification

All 58 local CTest executables pass. The loop test also passes with GCC and
ASan/UBSan, including every callback-failure prefix and cursor publication.

The private owned-data probe joins the checked texture/control keys, sorted
records, ordered dispatch and real-camera view transition for all 26 prepared
intro groups. It preserves every record and checks reset-only exhausted calls.
The single-view partition, frame word, pass rectangle and backend availability
are explicit test conditions, not a claim about original first-frame admission.
No retail data or expected retail vectors are published in the tests.
