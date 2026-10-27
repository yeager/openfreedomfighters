# Scene loading boundary

Scene loading must use bounded project-owned readers, validate references before publication, and retain the source data for the lifetime of any objects that depend on it. Failed loads must not publish partial runtime state.

Normal scene activation and rendering are not yet verified. This document does not publish original directory layouts, resource names, reference values, object identities, or native loader behavior.
