# Picture view transitions

`PictureViewTransition` represents one allocated ordinary renderer view. Its
last-clear frame word starts at zero and persists across passes. It is not a
camera, authored resource index, four-bit key value or global frame counter.
The caller must resolve the actual associated camera through the live view
intermediate before calling it.

## Ordered effects

Each call writes the supplied current engine frame into draw activity, submits
the viewport twice, and configures fog. Only then does it compare the per-view
clear guard with the frame word. Equal words skip clearing, not configuration.
An unequal word is stored before checking camera clear-suppression bit `0x8000`;
a suppressed clear therefore still consumes the guard for that frame.

Control zero clears color/depth using camera background (only packed word one
normalizes to zero). Control five clears depth only. Other controls clear
color/depth with color zero, except control four, whose separate border path is
unsupported and rejects. Depth clears also clear stencil when the caller's
actual attachment supports it. Clear uses depth one, stencil zero and the
current viewport, not an assumed full-swapchain rectangle.

Viewport input is a nonnegative, nonwrapping stored unsigned rectangle. Width
and height differences are converted to binary32 before separate products and
offset additions. Camera X/Y are bounded below by zero; spans are bounded above
by one. Each final component truncates independently to an unsigned integer.
Finite nearest-even arithmetic, representable coordinates and positive integer
extents are explicit native policies. No full-range camera clamp is introduced.

## Shared fog state

Zero global fog control or camera bit `0x80000` requests suppression. When the
suppression latch is clear, tracked fog enable becomes false and a backend
disable is submitted only if it was previously enabled. An already-set latch
produces no enable write. The operation never enables fog implicitly.

Start/end are current far times the camera's retained authored fractions, with
separate binary32 products. A zero end product becomes current far. Configuration
stores those distances and opaque base color, then submits color, vertex LINEAR,
table NONE, start and end in that order.

The shared `PictureRendererFogState` feeds subsequent material requests.
Configuration updates its base color but deliberately preserves tracked color,
additive color and special color. Backend fog color may differ from tracked
color. Treating those as one variable breaks later material comparisons.

## Integration boundary

The adapter executes ordered backend hooks; it does not manufacture admission
or draw pixels. Hook exceptions retain their completed prefix, including an
already-consumed clear guard. The caller must abort the failed frame. Missing
hooks, unsupported inputs and reentry reject before additional effects as
native safety policy. Hooks must preserve referenced lifetimes and state.

Normal startup still needs camera/view admission and a picture GPU executor.
The [ordered dispatcher](PICTURE_ORDERED_DRAW_LOOP.md),
[shared reset](PICTURE_DRAW_RESET.md) and
[renderer frame lifecycle](RENDERER_FRAME.md) are implemented components, not yet
the complete startup coordinator. A new engine's frame word starts at one, but
earlier qualifying renderer invocations can advance it before the intro; this
adapter accepts the current word rather than hardcoding a first-frame value.

The [SDL clear executor](SDL_PICTURE_CLEAR.md) now provides a separate bounded
GPU operation for a clear request. Connecting it to normal admitted intro views
is still required; the diagnostic runtime's full-target background is unchanged.

Public tests use independent inputs. Retail-source joins remain private and
conditional on explicitly supplied pass/backend state, not measured admission.

The private probe passes with the owned intro camera's real fractions, viewport
and background. The transition tests verify the CPU-to-hook contract; GPU clear
verification and its tested formats are described in the SDL clear executor page.
