# Clean-room protocol

OpenFreedomFighters separates research from implementation so that repository code is independently written from behavioral specifications.

## Roles

1. **Researcher:** observes a legally acquired copy, records behavior, file-layout facts, API boundaries, hashes, black-box test vectors, and concise interface specifications. The researcher does not publish disassembly, decompiler output, original code, dialogue, textures, models, audio, or other expressive content.
2. **Specification reviewer:** removes implementation-shaped expression and checks that a specification describes inputs, outputs, state changes, edge cases, and tests only.
3. **Implementer:** works from approved specifications and public platform documentation. Implementers must not consult unpublished decompilation or disassembly for the component they implement.

For a solo contributor, these roles must be separated by artifacts and time: retain private research notes outside the repository, produce a behavior-only specification, then implement from that specification in a separate session. Independent review is preferred before merging high-risk components.

## Permitted repository material

- independently written source code and tests;
- facts, measurements, hashes, ABI names, and high-level format descriptions;
- small synthetic fixtures created from scratch;
- scripts that inspect a user's local installation without redistributing it;
- links and citations to public documentation.

## Prohibited repository material

- original binaries or assets, including extracted archive members;
- disassembly/decompiler dumps or mechanically translated original code;
- original symbol/debug databases, leaked source, SDKs, or confidential material;
- dialogue or localization text copied from the game;
- patches containing substantial original machine-code sequences;
- credentials, Steam authentication material, or DRM circumvention.

## Static recompilation boundary

The project may translate observed machine behavior into an independently authored intermediate specification and native implementation. It must not ship the original executable, embed its code sections, bypass ownership checks, or operate without the required retail data. Where legal uncertainty exists, work stops at interoperable format/API documentation pending review.

Contributors are responsible for checking the laws applicable in their jurisdiction. This document is an engineering policy, not legal advice.

