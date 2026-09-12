# Graphics settings persistence

The functions in `off/settings/graphics_settings_store.hpp` persist only
`RequestedGraphicsSettings`: user intent, not resolved display state. The
version-1 document deliberately excludes `EffectiveGraphicsSettings`, fallback
information, and every `GraphicsCapabilities` value.

Loading accepts exactly version 1 and exactly the nine documented fields. Bad
syntax, duplicate or unknown fields, unsupported versions, invalid enum values,
and invalid dimensions or render scales return `invalid`; loading never rewrites
the file. Missing files are separately reported.

At runtime, SDL supplies the per-user application-preferences directory and the
file is named `graphics.settings`. If SDL cannot supply an absolute directory,
persistence is disabled for that launch; the application does not fall back to
a home-directory or environment-derived location. Saving validates settings,
then exclusively creates a sibling temporary file
(POSIX `O_CREAT|O_EXCL|O_NOFOLLOW`, Windows `CREATE_NEW`) and retries a bounded
set of collision-safe names. It flushes the temporary file, atomically replaces
the destination (POSIX rename or Windows `MoveFileExW`), and on POSIX flushes
the parent directory after the rename. Thus readers see either the old complete
file or the new complete file; after a successful save the replacement has been
requested durably from both the file and its containing directory. The caller
owns the config-directory policy and saves only after the user has confirmed a
successful graphics transaction.

Startup loads only a valid document. Missing, malformed, unsupported, or I/O
failed documents leave conservative defaults in place and are not repaired or
rewritten. An explicit `--mode` is a one-launch profile override: it takes
precedence over a saved profile but does not write on startup. If the user later
confirms an F10 change, the confirmed request is saved, including its selected
profile. A failed save never rolls back an already confirmed live graphics
configuration; it simply cannot be restored at a later launch.
