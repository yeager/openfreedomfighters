# Normal startup scene host

The scene host should own one active scene and route input, updates, audio, rendering, and teardown through that lifetime. Startup presentation alone does not establish scene activation.

CPU preparation and GPU work must remain on their appropriate owning threads. Scene replacement must retire the old scene safely, and partial startup failure must release acquired resources. The normal intro-to-menu path remains incomplete until it reaches a live, repeatable scene loop.
