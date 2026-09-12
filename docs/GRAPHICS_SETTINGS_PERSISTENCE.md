# Graphics settings persistence

The functions in `off/settings/graphics_settings_store.hpp` persist only
`RequestedGraphicsSettings`: user intent, not resolved display state. The
version-1 document deliberately excludes `EffectiveGraphicsSettings`, fallback
information, and every `GraphicsCapabilities` value.

Loading accepts exactly version 1 and exactly the nine documented fields. Bad
syntax, duplicate or unknown fields, unsupported versions, invalid enum values,
and invalid dimensions or render scales return `invalid`; loading never rewrites
the file. Missing files are separately reported.

Saving validates settings, then exclusively creates a sibling temporary file
(POSIX `O_CREAT|O_EXCL|O_NOFOLLOW`, Windows `CREATE_NEW`) and retries a bounded
set of collision-safe names. It flushes the temporary file, atomically replaces
the destination (POSIX rename or Windows `MoveFileExW`), and on POSIX flushes
the parent directory after the rename. Thus readers see either the old complete
file or the new complete file; after a successful save the replacement has been
requested durably from both the file and its containing directory. The caller
owns the config-directory policy and should save only after the user has
confirmed a successful graphics transaction.

SDL startup does not currently establish a portable user configuration location
or a confirmed-settings save boundary. It therefore does not load or save this
store yet; integration belongs with that ownership contract.
