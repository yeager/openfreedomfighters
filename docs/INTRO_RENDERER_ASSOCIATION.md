# Intro renderer association boundary

Scene resources, renderer resources, traversal eligibility, draw submission, and
GPU presentation are separate interfaces. Retaining a resource does not by
itself make it drawable. A renderer must receive an explicitly validated
submission record and preserve its resource lifetime through completion.

## Producer boundary

The project must define a bounded producer that connects prepared scene data to
ordered renderer submissions. It must specify record ownership, identity,
validation, and failure behavior without guessing serialized layouts or using
array positions as identities. Until that producer is implemented, normal
startup must keep the path disconnected.

## Verification

Use project-authored synthetic data to test ownership, ordering, invalid input,
and submission failure. Do not publish installation-derived joins, record
inventories, or private analysis. Resource availability or a preview camera
alone does not prove presentation or parity.
