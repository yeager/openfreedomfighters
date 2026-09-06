# Renderer frame lifecycle

The engine owns one unsigned 32-bit renderer frame word, initially 1. A view
retains its own last-clear word, initially 0. Neither is an application-update
counter or a timer for skipping the intro.

The recovered renderer operation has these boundaries:

1. If the engine is not running or the renderer is not initialized, return
   without backend work or a counter increment.
2. Attempt device-scene admission.
3. On success, traverse the renderer, run admitted post-render work and end the
   device scene, in that order. On failed admission, skip all three.
4. Run the renderer completion hook in either case, then increment the shared
   word with unsigned wrapping arithmetic.

Drawing therefore sees the pre-increment word. Multiple qualifying renderer
invocations during one application update advance the same counter independently.
A failed device admission also advances it; an update with rendering disabled
never enters this operation.

This establishes a producer for `PictureViewTransition`'s frame input. It does
not establish the counter at the first intro picture: earlier renderer calls
must be retained. It also does not admit a camera, prepare an ordered draw list,
or provide successful device admission on behalf of an absent backend.

The native coordinator requires explicit callbacks. Callback exceptions retain
completed effects and abort the operation; they are not interpreted as ordinary
device admission failure. This exception policy is a native safety boundary,
not a recovered original exception-handling claim.
Native guards reject recursive use of either the coordinator or its shared clock.
They are not synchronization primitives: callers must serialize renderer calls
and keep callback and clock storage alive throughout the operation.

`RendererFrame` and its caller-owned `RendererFrameClock` provide this lifecycle.
They are distinct from `RendererFramePass`, which snapshots registered renderer
states for their frame/maintenance callbacks; that pass does not own or advance
the engine clock. A clock is shared across renderers, not reconstructed per view
or scene. Explicit adoption of a known existing word supports retained state;
it does not establish an unknown startup history.

The GPU witness test connects the coordinator to `PictureViewTransition` and
`SdlPictureClear`. With an explicitly admitted synthetic view, it checks that
failed device admission advances the clock without clearing, repeated transitions
within a render call clear only once, and the next admitted frame uses its new
word. This tests actual attachment effects, not real intro admission.

Unit tests cover callback order, both outer gates, missing hooks, shared clocks,
unsigned wrap, each callback failure prefix and recursive calls. All 62 local
CTest executables pass. The connected Vulkan witness also passes targeted GCC
and ASan/UBSan runs with SDL's offscreen backend.
