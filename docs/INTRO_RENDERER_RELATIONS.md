# Intro renderer resource relations

This page describes only the public ownership boundary. Resource identities and mappings derived from a retail installation are not published.

Renderer-facing relations must resolve through canonical scene-owned resources and remain valid for the owning scene lifetime. Resource allocation, relation membership, draw-record production, and backend submission are separate stages; success at one stage does not imply completion of the next. Unsupported relations must fail closed.
