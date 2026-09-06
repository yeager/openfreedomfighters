# Live resource flags

`IntroRuntime` retains one optional complete resource flag word and a separate
typed resource-context handle per canonical resource slot. Resources can exist
without an associated owner. Unknown state stays unknown. Authored
flags, camera-owner flags and owner aggregate flags are different values.
Picture flag queries and PreviewCamera resource views use this same storage,
including after a dynamic camera grows the resource collection.

`assign_resource_state` publishes state supplied by a real producer. It is not
the game's flag setter and performs no attachment or registration work.

## Mutations

`mutate_resource_low_byte` replaces the target's low byte with
`((flags & ~clear_mask) | set_mask) & 0xff`, preserving higher bits. Requested
low set bits propagate to every resource ancestor. Clears do not propagate;
clearing the root does not change its children. Context, pose and owner flags
are untouched.

`set_resource_flags_no_maintenance` implements the construction-time setter
branch that needs no registration or renderer maintenance:

- Resource bit `0x2000` must not change.
- When either mask touches `0x8000`, actual allocation mode must be off or
  actual renderer-resource maintenance must be suppressed.

It applies the low-byte operation first, then the remaining masks to the current
target word. Set wins over clear. Full replacement therefore still propagates
requested low bits to ancestors; it is not a plain assignment.

Unsupported maintenance and unknown required words fail before mutation.
Unknown ancestors are irrelevant to a clear-only request. This preflight is a
native safety policy, not a claim about original failure handling.

## Loader boundary

Normal startup calls `construct_root` before admitting authored resources. It
executes single allocation, the ZROOM owner factory, RootGroup enrollment and
its immediate initializer. At this boundary the root's actual flags are
`0x09000000`, with identity pose, no parent/context and an empty live child list.
Prepared authored links are detached; later attachment must build the real list.
An existing completed root is reused without resetting its state.

Normal CPU preflight then calls `begin_source_loading_without_engine_renderer`.
This explicitly resets retained scene progress once as a native load-begin
policy, applies the first directory row's stage-three progress operation
(`0.8`, admitted only above the retained value plus `0.002`), and allocates the
initial scope. This is not an original constructor default or proof of the
original cold reset caller. No engine GPU renderer exists at this native stage;
the separate splash renderer does not supply that identity. A future loader
running with an engine renderer must execute its real progress drawing and
presentation services, not reuse this absent-renderer entry point.

Every subsequent row uses the same retained progress and its actual directory
index/count. Stage three spans binary32 `0.8` to `0.9`; each arithmetic step is
rounded to binary32. A new candidate must exceed retained progress plus `0.002`.
Progress is not reset between rows and does not itself construct an owner.

The owned intro's first scope contains 20 resources, partitioned by its count
table. Inactive batch construction gives each a distinct native resource ID,
identity pose, positive-zero position, no parent/context and flags `0x09000000`.
They remain ownerless. Reserved source catalog handles are not live owners;
owner/resource lookup and camera registration reject unconstructed owners.
Canonical flag mutation still works on an allocated ownerless resource.

At that boundary only the first scope is allocated. Its partition cursors remain unconsumed
until owner construction starts. Later batches must be interleaved with source
factories and attachments, not eagerly constructed for the entire directory.
`allocate_initial_source_scope` is also exposed as an explicit stage operation;
it does not itself execute preceding progress or claim complete loading.

## First authored group

The next stage constructs the first directory row's group on its existing batch
resource. It consumes the category-zero cursor before the factory and retains
the authored name. Owner flags and resource flags stay separate. Attachment
links that same resource to ROOT; it does not allocate a replacement resource.

The loader retains its resource-handle list, directory-index mapping and deferred
reader queue. Queuing the source's deferred blob does not execute its reader.
The count-group selector advances, but this row does not enter a child scope.
The other initial resources remain ownerless, and the scene is not ready.

This stage is restricted to the reviewed first-row shape and live fresh-root
state. It is not a generic group factory or a replacement for subsequent source
readers, component creation and loader-tail processing.

The concrete class's notification sequence is application-owned, shared across
hosts and incremented with unsigned 32-bit wrap. Destruction does not decrement
it. Native startup explicitly registers the implemented group class once;
missing registration rejects before slot consumption. This limited registration
policy does not implement the original pre-GMS class-list preparation, which
resets all registered sequences and resolves base-class links. It must not be
mistaken for a per-host reset or a count of currently living owners.

## Window and Language groups

Normal startup next constructs the distinct Window and Language classes. Window
consumes the next resource in the initial scope and attaches after the first
group under ROOT. Entering it advances the count selector to two. Only after
the next row's progress operation does the loader allocate count-group two;
Language consumes that scope's first group slot and attaches to Window. The
current hierarchy parent becomes Language, while count-group three remains
pending. The owned intro has 23 resources across the two constructed scopes,
three attached owners and three deferred-reader items at this point.

Window's canonical pending-visibility float starts at positive zero. Its real
`Show2d` console lease binds that storage; registration does not write `-1` or
execute input setup. The scene's `rWindows` property stores the same canonical
Window resource handle as type 16. Insertion removes an existing exact key and
replaces it, preserving unrelated properties. Native typed handles replace the
original four-byte payload representation; they are not authored references.
Embedded NUL keys are rejected by the native API.

Window and Language have separate application-owned construction notification
sequences. Their factories do not run deferred readers, create input maps or
construct Picture components. Resource flags remain `0x09000000` on this bounded
path, distinct from each group's owner flags. The remaining directory and loader
tail are still required before scene activation.

## Picture and attachment construction

The next two rows construct Picture owners on their supplied category-one
resources. The first allocates count-group three and attaches under Language.
The second restores Window's existing count-group-two cursor, without allocating
another batch, and attaches after Language. The owned intro now has 24 allocated
resources, five attached owners and five queued readers. No later owner or
deferred reader runs in this stage.

These live Picture owners are separate from prepared image and descriptor data.
Their constructors retain white color, alpha 255, unit size scales and an absent
backing/submission cache. They do not install authored materials or invalidate a
transform. Deferred reading must populate that state before any draw admission.

The hidden first Picture constructs `ZGEOM_Center` with argument 1. Its hidden
resource prevents component enrollment and owner-mask changes. The visible
second Picture constructs `ZWINPIC_FadeToBlack` with argument 0, idle fade state
and zero deadlines. Its requested mask is `0x35`; enrollment admits `0x10` and
appends the real component handle to the ordinary pending queue. Neither runs an
initializer, ordinary update or fade event. Registration at the non-room ROOT
requires the explicitly absent engine renderer used by CPU preflight.

Both Picture instances advance the same owner-class notification counter.
Center and Fade each advance their own component-class counter after attachment
and enrollment. Their serials and scheduling phase come from the retained common
component lifecycle; no per-Picture or per-scene reset substitutes for it.

Before ROOT construction, source event names are declared in GMS table order.
The one-based source mapping stores the returned scene identities, not row
numbers; entry zero remains absent. ASCII case variants reuse an identity.
Fade declares `FadeIn` then `FadeOut` against this same retained scene registry.
Lazy camera-name reservations do not advance the dynamic counter. Native checks
reject conflicting reverse identities, capacity overflow and embedded NUL names.
The registry owns its names and does not dispatch events. The host exposes
declarations and read-only inspection, not a clear operation that could invalidate
already prepared source mappings.

## Authored Camera construction

The next row consumes Window's existing category-three slot without allocating
another scope. It binds the concrete Camera owner and appends it after the
Picture in Window's child list. Its name, canonical resource and retained
Camera-class notification sequence are preserved. This leaves 24 allocated
resources, six attached owners and six queued readers in the owned intro.

The live camera uses the ordinary constructor defaults, not the prepared
authored-source projection. Its runtime flags are `0x20`, priority is zero,
near/far are 5/20000, viewport is `(0,0,1,1)` and renderer dimensions are zero.
Its context refers to ROOT; its separate resource parent is Window. The authored
resource hide produces `0x09000400` without clearing the camera's enabled bit.
Identity admission compares every orientation word bitwise, including signed
zero, before skipping transform dirtying and queue work.

No component, event declaration, renderer registration, dimension notification
or source-reader operation runs for this row. The prepared camera remains an
offline compatibility object. Once actual source loading begins, camera access
rejects until the real factory completes, and all subsequent camera accessors
use that same live instance. A prepared-only Window projection does not count
as live initialization. Cold loading also rejects a pre-registered camera rather
than retaining an invalid renderer association across the transition.

RootGroup retains its `DisplayName` floating-point descriptor and `RootControl`
input-map registration. Its ordinary membership remains pending, not merged.
The root's separate enabled marker does not enable a camera or set a resource
initialized bit. Global phase one repeats the same initializer and map reference
operation. Its ordinary input processor is still missing and fails by name.

These operations require an established live parent chain. The prepared GMS
hierarchy alone does not prove that original attachment services have run.

## Second Window scope

Normal startup continues through source row 41. Row 6 restores ROOT as parent
and constructs another Window, with an independent `Show2d` lease. Its
`rWindows` property replaces the previous resource value without destroying the
first Window. Owner-addressed camera access uses stable records; first-cut
convenience accessors still select the first Window and Camera.

The new scope contains three Pictures, thirteen Characters, one Camera and
eighteen Lists. Constructors bind supplied resources before notification and
attachment. Characters retain their own concrete class and control value 64;
their common visual defaults do not decode text or build geometry. Lists start
empty: deferred source references are not resolved by their constructor.

Character positions run the actual directory transform operation. A changed
pose copies position and basis, preserves resource dirty bit `0x00100000`, and
notifies the retained position service. It does not invalidate Picture caches.
The fresh scene starts with suppression zero and derives immediate mode from
the selected filename (`.wld`/`.wl2`, case-insensitive). Ordinary load begin
disables collection and increments the retained suppression word with 32-bit
wrap. For the selected `.gms`, notifications return on disabled collection
without inspecting suppression or touching queue, bounds or spatial state.
An immediate-mode scene is not admitted by this bounded construction path.

CharFader initializes two clock words and reuses `FadeIn`/`FadeOut`. Its fade mode
remains unavailable until a later producer. LogoFade initializes its clocks but
declares no events; both event IDs and mode remain unavailable. External command
attachments retain raw argument `0x3f800000`, independent of default priority 1.
Duplicate attachments create distinct instances and advance shared counters.
Hidden attachments still construct but do not enter the ordinary pending queue.

The owned-data probe verifies 59 batch resources plus ROOT, 42 attached source
owners and queued readers, 50 constructed components and 15 ordinary pending
additions. Synthetic integration fixtures exercise the same scope with different
initial resource counts and independently authored names/data. Position-service
tests also check that disabled notifications preserve an existing nonempty queue.
The readers, global initialization, camera registration and rendering remain
pending; these counts are construction evidence, not a playable-intro claim.

## Following visual scope

The next complete scope constructs an ordinary Group at row 42 and five visual
owners of concrete class `0x0020003a` at rows 43–47. The Group joins ROOT after
the second Window. Its five-slot batch is allocated only when entering its first
child; prior scope cursors and owner identities remain intact.

These visual owners use the shared visual-base defaults but are not Pictures.
No source geometry, material, attachment or rendering state is produced by their
constructor. Their separate class notification counter advances for each owner.

The directory metadata setter runs before transform setup. For a fresh resource,
the old metadata word is zero, so there is no renderer cleanup. A changed word
sets resource dirty bit `0x00100000`; an unchanged word preserves flags. Metadata
does not notify the position service. All five transforms are unchanged here,
so four nonzero-metadata resources retain `0x09100000` while the zero-metadata
resource retains `0x09000000`, without any position queue activity.

Owned-data verification now reaches 64 batch resources plus ROOT and 48 source
owners/readers. Component count, events, scheduling phase and ordinary pending
additions are unchanged from the second Window scope. Independent fixtures use
different names, metadata words and initial allocation counts. Row 48 begins
the next nested scope; its Room/context, collection and animation behavior is
not admitted by the preceding non-room construction path.
The real source path must preserve its first setter, attachment, then second
setter using the post-attachment word. Remaining source readers and loader
services are not replaced by constructor constants or empty callbacks.

Tests use independently constructed states to check ancestor propagation,
maintenance gates, picture views and subsequent DefaultCam hide inheritance.
They do not establish the retail root's post-load flags. The explicit `root_ready`
and `initial_scope_ready` stages block DefaultCam fallback until actual authored loading is implemented;
the fresh construction result is not accepted as the later loader result.
