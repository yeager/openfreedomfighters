# Startup-menu presentation audit

Source review: 2026-09-13.

## Current native surface

The checked `FF-StartUp` archive already supplies one safe static inspection
surface: `--diagnostic-startup-graphics`. It decodes the six source-selected
images, retains the recovered resting-state traversal, expands the 21 visible
picture instances into 77 source-backed quad submissions, and draws them with
the diagnostic renderer. It is intentionally an inspection view, not a menu:
its projection, blend policy, depth policy, sampler choice and output mapping
belong to the project diagnostic renderer.

Normal startup does not select or present that surface. It retains the checked
BootMenu hierarchy and its deferred source block, then presents the separately
admitted static intro fallback while the intro and scene-transition paths are
incomplete. The F10 settings overlay is project-authored and must not be used
as evidence that the authored main menu has been activated.

## Evidence retained

The startup graphics extractor establishes all of the following without using
resource names, image ordering, pixel similarity, or exported retail assets:

- one structurally unique menu subtree;
- eight candidate rows, with the authored hidden row excluded in the resting
  state;
- seven persistent backgrounds plus fourteen chrome instances at state `0x01`;
- 21 visible picture instances, 77 source-backed one-quad submissions, and six
  paired decoded textures;
- source hierarchy traversal and immediate group-emission order; and
- each picture's opaque authored control values and local transform chain.

The GPU diagnostic validates those identities again and uploads the six images
only from the user's archive. No retail pixel, text, identifier, or capture is
written into the repository.

## Why it cannot become the normal menu yet

The recovered rows are only a graphics subset. The current evidence does not
identify a live BootMenu root, activation event, active row, focus target,
navigation order, action meaning, text reader result, window/camera transform,
or the raster-state producer. Reusing the diagnostic's generic-fit projection
would visibly produce a static image but would falsely claim authored layout.
Likewise, treating state `0x01` as an active menu state would turn a recovered
visibility mask into an unobserved state-machine decision.

The native renderer reflects this boundary: it accepts either an explicit
source-only diagnostic `SceneGpuPlan` or a retained normal intro session. A
normal session has no scene draw producer, and the startup textures are kept
uploaded but unbound. Introducing a parallel normal menu draw would bypass the
unrecovered scene replacement, menu reader, and transition lifecycle.

## Next implementable work

The next menu implementation must be driven by a reviewed, source-free
observation of an isolated original run. It needs to establish, in order:

1. the terminal intro request and the destination scene identity;
2. the owner/component reader path that produces the active BootMenu root;
3. the virtual-window/camera transform and ordered picture submission state;
4. initial focus, input transitions, and action dispatch; and
5. the lifecycle that replaces intro rendering with menu rendering and tears
   both down without stale events or audio.

Only after items 1--3 are established may the existing 77-submission asset be
connected to a normal presentation pass. Items 4--5 are required before that
pass is called a responsive main menu. Until then, the diagnostic command is
the largest safe source-backed menu presentation step.
