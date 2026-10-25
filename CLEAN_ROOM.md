# Clean-room protocol

OpenFreedomFighters separates research from implementation so that repository code is independently written from behavioral specifications.

## Roles

1. **Researcher:** observes a legally acquired copy and records private behavior notes and test observations. The researcher does not publish retail-derived data or analysis artifacts.
2. **Specification reviewer:** removes implementation-shaped expression and checks that a specification describes inputs, outputs, state changes, edge cases, and tests only.
3. **Implementer:** works from approved specifications and public platform documentation. Implementers must not consult unpublished decompilation or disassembly for the component they implement.

For a solo contributor, these roles must be separated by artifacts and time: retain private research notes outside the repository, produce a behavior-only specification, then implement from that specification in a separate session. Independent review is preferred before merging high-risk components.

## Permitted repository material

- independently written source code and tests;
- independently authored, non-expressive behavior and interface specifications that do not reproduce retail values or payloads;
- small synthetic fixtures created from scratch;
- scripts that inspect a user's local installation without redistributing it;
- links and citations to public documentation.

## Prohibited repository material

- original binaries or assets, including extracted archive members;
- retail-derived data, including executable/resource fingerprints, inventories, payload measurements, disassembly/decompiler dumps, and mechanically translated original code;
- original symbol/debug databases, leaked source, SDKs, or confidential material;
- dialogue or localization text copied from the game;
- patches containing substantial original machine-code sequences;
- credentials, Steam authentication material, or DRM circumvention.

## Static recompilation boundary

The project may translate observed machine behavior into an independently authored intermediate specification and native implementation. It must not ship the original executable, embed its code sections, bypass ownership checks, or operate without the required retail data. Where legal uncertainty exists, work stops at interoperable format/API documentation pending review.

Contributors are responsible for checking the laws applicable in their jurisdiction. This document is an engineering policy, not legal advice.

