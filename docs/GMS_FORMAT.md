# Object-source resource handling

The project reads object-source resources from the user's installed game data.
The parser provides bounded access to object records, relationships, and
references needed by the runtime; it does not publish source payloads or copied
asset data.

Parsed resource identity, runtime object identity, and renderer identity remain
separate until a source-backed runtime join is established. The diagnostic
renderer must not treat retained source transforms as verified world poses.

This document intentionally omits game-specific layouts, values, symbols,
offsets, corpus measurements, and executable-analysis results. The parser
implementation and independently authored fixtures are maintained in the source
tree.
