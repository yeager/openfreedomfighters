# Stereo audio output

The audio backend should convert validated project-owned channel state into platform output while keeping audio independent from simulation results. This page omits source-derived gain values, record layouts, and channel inventories.

The service must handle initialization failure, bounded buffering, stop, and teardown. No retail audio or private cue mapping belongs in version control.
