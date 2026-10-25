# Scene directory construction

Status: implementation in progress. The runtime still contains fixed prefix
stages; this contract describes their replacement. It does not establish scene
activation, intro playback, or renderer admission.

## Input and ownership

Construction consumes the local parser's directory, validated parent references,
and allocation partitions. Directory position identifies an input record; it
must not determine its class, flags, component list, or gameplay role.

Each record receives exactly one canonical owner/resource mapping. A parent
reference must resolve to an already constructed owner in the same scene. An
absent parent refers to the scene root. Allocation follows each record's parsed
pool and class partition, independently of parent selection. Group construction
and entry into a child pool are separate properties.

Supported class handlers consume the local record's transform, properties,
attachments, and deferred-reader inputs. Unknown classes or unsupported records
must identify the failed input and leave the scene in a failed state. Partial
construction must remain destructible; it must not enable updates or rendering.

## Scene roles and completion

Camera, window, controller, and cut references resolve through validated local
references and typed bindings. Where a role requires uniqueness, missing or
ambiguous bindings fail explicitly. Accessors return the canonical owner;
they must not create a second mutable copy for a special directory position.

Directory construction completes only when every input record has a valid
mapping and its supported construction work has completed. Reader coverage,
component initialization, scene activation, audio, and drawing remain separate
requirements after construction.

## Acceptance checks

Use independent synthetic scenes with different directory sizes, sibling order,
component positions, and nested groups. Include a root-level group that does not
enter a child pool, followed by a sibling, to detect accidental nesting. Verify
canonical parent/owner identities and deferred-reader order, not just totals.

Reject missing or forward parent references, invalid partitions, exhausted
allocation slots, unknown class handlers, ambiguous required roles, and malformed
attachments. Verify failure cleanup and that no scene activation occurs after a
partial construction failure. A private owned-installation run must additionally
confirm that supported normal startup still loads before claiming compatibility.
