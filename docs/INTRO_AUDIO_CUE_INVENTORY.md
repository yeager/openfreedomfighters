# Intro audio integration boundary

Cue identities, source mappings, and observed playback details remain private and are not published in this inventory.

The runtime should resolve authored audio references through the owning scene, start and stop channels through the audio service, and release source resources only after active playback has ended. Missing optional audio must not prevent the application from starting. Public diagnostics should report generic readiness only.
