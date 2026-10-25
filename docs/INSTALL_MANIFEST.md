# Local installation validation

The application reads game files only from the user's installed copy. Archive,
path, and resource parsers validate structure before data reaches runtime code.
Original files, filenames, hashes, and decoded content are not published by
this document.

Optional soundtrack files are independent of startup. Missing or unreadable
optional audio must leave the original game-audio path available.

Any future local integrity baseline must be created and stored outside the
repository from the user's own installation. It must clearly distinguish local
consistency checking from an official publisher authenticity manifest. This
document intentionally omits installation inventories, file sizes, hashes,
resource names, cue mappings, and private analysis results.

## Required behavior

The local content fingerprint must include each required file's relative path,
observed byte length, and full-byte digest in a deterministic order. Directory
enumeration order and optional soundtrack changes must not alter required-data
identity. Required-content changes must invalidate dependent structural-audit
caches and prevent silently reusing saves bound to different content.

Hashing and structural validation establish local identity and supported
structure. They do not establish publisher authenticity or complete gameplay
compatibility. User-facing status must preserve that distinction.

Interior symbolic links, ambiguous path spellings, and failed required-file
reads must fail validation. Optional-file failures must produce diagnostics
without blocking required data. Cancellation must stop discovery and hashing
without returning a successful partial inventory. The installation must remain
stable while it is validated and used.

## Verification

Use independently authored temporary trees to check deterministic identity,
required-file mutations, optional-file isolation, unsafe paths, cancellation,
and structural failures. Fixtures must vary their sizes and ordering without
reconstructing a retail inventory. A private owned-installation run is also
needed to establish that normal startup still accepts supported local data;
synthetic manifest tests alone do not prove that.
