# Cut timeline boundary

This page no longer publishes source-derived timing values or command data.

A scene timeline must use the owning application clock, preserve pause and discontinuity handling, and keep authored event dispatch separate from rendering cadence. A redraw must not advance scene time. Timeline activation, updates, completion, skip, and teardown require a reviewed behavior contract and independent synthetic cases.
