# Intro startup activation boundary

`IntroStartupActivation` is a strict, caller-wired ordinary-startup sequence.
It runs the post-construction reader bracket, the outer loader tail through the
saved-resource services, a supplied concrete global-lifecycle call, and
MovieControl phase two, in that order.

The class is intentionally not connected to normal application startup.  It
does not parse named/global or renderer payloads, provide association/resource
services, create a renderer view, activate a cut, execute an update, draw, or
present.  Every parser, resolver, scene service, lifecycle callback and
MovieControl phase-two callback remains caller supplied; a missing or failing
boundary leaves activation failed.

`IntroOuterLoaderTailReadiness` is a source-backed preflight report, not an
activation service. It records the retained source section sizes and which
concrete boundary types remain to be supplied; it never makes the loader tail
runnable by itself.

Successful completion only means MovieControl has received its phase-two
deadline/setup boundary.  A later admitted ordinary update must strictly pass
that deadline before cut preparation, and rendering still requires separate
camera, state/view, device and backend admission.

## First-cut admission boundary

The recovered first-cut route is fail closed: ordinary load and global lifecycle
must complete, MovieControl phase two assigns its absolute scene-clock deadline,
and a later admitted event-16 update must be strictly past that deadline. Only
then does preparation complete, the active latch and playback baseline change,
and `CutSequence_Start` run synchronously.

The normal inactive-list receiver then selects the `MainCamera` property branch.
For the recovered first option, it first sweeps registered cameras and then
resolves and registers the explicit requested camera. A nonzero named-camera
branch remains unsupported. This proves neither a renderer view, GPU submission,
audio playback, presentation, nor cut completion.
