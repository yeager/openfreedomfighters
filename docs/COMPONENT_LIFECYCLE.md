# Scene component lifecycle

This page no longer publishes retail-derived component identities, state flags, callback order, or numeric values.

Components must be owned by a scene session and move through validated lifecycle stages. Update and teardown are distinct from construction and resource loading. Unsupported callbacks must fail closed; partial initialization must release acquired resources without presenting the scene as ready. Use synthetic fixtures for ordering, reentry, failure, and cleanup.
