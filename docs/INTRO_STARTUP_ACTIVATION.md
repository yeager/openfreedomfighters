# Intro startup activation boundary

`IntroStartupActivation` is a caller-wired startup ordering model.
It runs the post-construction reader bracket, the outer loader tail through the
saved-resource services, a supplied concrete global-lifecycle call, and
MovieControl phase two, in that order.

The class is not connected to normal application startup. Its runtime tail now
has concrete supported named-reference and renderer-relation readers; scene,
List-association and saved-resource services remain required from the caller.
The ordering model itself does not create a view, activate a cut, run an
ordinary update, draw or present. Missing or failing boundaries leave it failed.

The production MovieControl factory now installs a source-checked phase-two
callback using the same retained controller and application clock/audio state.
Its service binding requires both readers and calls no service. Phase one and
whole-scene lifecycle admission remain incomplete.

The future normal activation continuation must adapt this model to resume the
already completed reader bracket. It must also avoid the model's separate
MovieControl phase-two call after a global pass that already dispatched the
factory-owned callback. Replaying readers or initializing a second controller
would not implement the original startup sequence.

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
