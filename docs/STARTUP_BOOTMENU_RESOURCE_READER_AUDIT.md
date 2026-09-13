# BootMenu resource and reader-chain audit

Source review: 2026-09-13.

## Confirmed source-backed chain

The supported startup path has one deliberately narrow, testable resource
chain. It begins with the checked `FF-StartUp` package and its parsed GMS
directory. `StartupBootSceneDirectorySource` rejects every ambiguous
`ZWINDOW_BootMenu` attachment and retains only the ordinary-window attachment
with finite parameter `1`. `StartupBootSceneRegistryFactory` then maps that
source owner into a fresh transaction-local registry handle while retaining the
checked package and scene lease. `StartupBootMenuComponentEnvelopeFactory`
requires that exact package, directory, registry transaction, and owner
mapping before copying the corresponding deferred source block.

The envelope does not decode component semantics. Its only exposed inspection
is `StartupBootMenuProfileProbe`, which reports aggregate framing: body size,
value-kind counts, delimiter and continuation counts, encoded-value count, and
a tag-only digest. The probe does not reveal a source directory, object handle,
source byte, decoded value, string, identifier, or reader key. The existing
`--probe-startup-boot-profile` command runs this chain against a verified owned
installation and emits only those aggregates.

The lifecycle class `StartupBootMenuAdmission` is a separate, source-free
service boundary. It accepts the factory construction token, requires live
owner/component checks, and orders the common component reader before common
window initialization and retained routing. Its two identities are supplied by
opaque concrete services and cannot be caller-chosen numeric substitutes. A
successful receipt proves only that those supplied services completed in order;
it is not proof that the native reader was invoked or that the component has
focus, input, rendering, or scene authority.

## Presentation boundary

The six decoded startup images and their 77 source-backed diagnostic quad
submissions come from a different static graphics extraction path. That path
has structural source evidence for one resting visibility mask and submission
order, but no link from the BootMenu envelope or admission receipt to a live
draw producer. Normal startup therefore retains the startup textures without
binding them, and the normal world pass remains clear-only. The explicit
`--diagnostic-startup-graphics` view uses project-owned fit projection and GPU
state; it cannot be treated as an authored menu presentation.

No repository path proves an intro terminal event, destination startup scene,
scene-current transition, live BootMenu root, reader dispatch table, active
window/camera transform, picture raster state, initial focus, action routing,
or teardown ordering. In particular, the source block's existence and its
aggregate framing do not identify a component reader implementation or a
renderer pass.

## Result and evidence gap

No aggregate inventory beyond the existing deferred-block profile is added.
The profile is already source-backed, privacy-preserving, and covered by
negative tests for header-only, malformed, and framing-changed blocks. A new
inventory that claimed reader completion, BootMenu activation, or an
intro-to-menu presentation edge would need to fabricate one or more missing
runtime facts.

The next admissible evidence is a reviewed, source-free observation that ties
the terminal intro transition to the destination scene and then identifies the
actual BootMenu reader dispatch plus its initial draw producer. Until that
evidence exists, the only valid outputs are the retained aggregate profile and
the explicitly labelled static graphics diagnostic. Neither output authorizes
scene activation, input binding, focus selection, a menu draw on normal
startup, or a gameplay transition.
