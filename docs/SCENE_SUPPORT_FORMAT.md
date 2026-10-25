# Scene dependency handling

Scene dependency resources are parsed into bounded dependency records. The
reader validates names and references before the application considers loading
dependent resources. Resource contents and original dependency names remain in
the user's installation.

This document intentionally omits game-specific layouts, names, counts,
measurements, and payload details. Synthetic fixtures exercise malformed and
truncated inputs without copying original data.
