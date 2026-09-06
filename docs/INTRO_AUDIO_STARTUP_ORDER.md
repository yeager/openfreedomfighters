# Intro audio startup and frame order

These clean-room ordering constraints come from independently reviewed behavior
specifications. They are not a claim that the native startup loop implements
every stage below, or that a Windows runtime capture has verified the result.

## Startup

Scene startup initializes the sound backend before loading the scene file.
With preferences present, initialization requests categories 0, 1 and 2 in that
order, using mode 0 and the retained sound-effects, music and speech volumes.
Absent preferences skip those requests. Device initialization follows; only
success enables command processing. A failed device must not produce a start
acknowledgement.

Source loading retains SND/WHD links and attachment data. All sound-owner
pre-hooks precede the reverse global component phases. The SoundNotify duration
snapshot therefore reads the canonical record after owner preparation, not a
substitute calculated by an attachment constructor. SoundExtend parameter
application and SoundSegment's record changes do not themselves start playback.

The full cold-loading schedule is still incomplete. An ordinary scene frame and
MovieControl's phase-two clear/present service are different operations. Do not
use the latter as permission to execute the ordinary sound pump during loading.

## Admitted ordinary frame

After successful startup and frame admission, the relevant order is:

1. Receive available acknowledgements from the existing sound backend.
2. Update ordinary scene components, including SoundNotify.
3. Run the later scene position/bounds update.
4. Traverse renderers when rendering is requested.
5. Update the scene sound service: update its auxiliary service, receive
   acknowledgements again, then process prepared records and submit admitted
   channel commands.

Disabling rendering skips step 4, not step 5. A real acknowledgement received
at step 1 can affect that frame's component update. One first received at step 5
cannot retroactively affect callbacks that already ran. Neither receive invents
messages, and submitting a command is not an acknowledgement of its execution.

## Listener and environment prerequisites

Normal prepared-record processing requires a resolved listener. It first tries
the backend's explicit listener handle. Otherwise it asks the first renderer
for registered-camera collection element zero, after pruning unresolved handles.
This lookup does not test enabled state and is not a separate selected-camera
pointer, authored source number or MainCamera property. Without a listener,
normal record processing returns before admission.

Setting an explicit listener validates the live handle, stores it and clears
the three listener offsets. A failed lookup leaves the previous handle intact.
An ordinary camera's retained room/context reference initially points to the
actual synthesized scene root, independently of its resource parent. Later
assignments must survive; a null context falls back to the scene root.

For the reviewed concrete low-level manager, two unspecified original
environment-group header fields do not affect consumers. A native semantic
command model may omit them, but must preserve defined counts and typed entries.
In particular a type-3 entry with a null effect-data reference is still an entry,
not removal of the environment stage. This is not byte-exact reproduction of
uninitialized original command bytes or a rule for other managers.

## Native implementation boundary

The current host retains records, typed attachments and incremental streams,
with explicit owner preparation, stop and SoundExtend operations. Automatic
component admission, the loading-time listener history, complete environment
construction, output-channel processing and the two receive calls still need
to be connected. See [retained sound state](INTRO_SOUND_RUNTIME.md) and
[stream decoding](INTRO_AUDIO_STREAMING.md).

The application-owned [preview camera pointer update](PREVIEW_CAMERA.md) now
handles the reviewed mouse-motion path. Camera construction, live input routing
and normal component admission remain separate work.
