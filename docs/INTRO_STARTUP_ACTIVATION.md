# Intro startup activation boundary

`IntroStartupActivation` is a strict, caller-wired ordinary-startup sequence.
It runs the post-construction reader bracket, the outer loader tail through the
saved-resource services, a supplied concrete global-lifecycle call, and
MovieControl phase two, in that order.

The class is intentionally not connected to normal application startup.  It
does not parse named/global or renderer payloads, provide association/resource
services, create a renderer view, activate a cut, execute an update, draw, or
present.  Every parser, resolver, scene service, lifecycle callback and
MovieControl phase-two callback remains caller supplied; a missing or failing
boundary leaves activation failed.

Successful completion only means MovieControl has received its phase-two
deadline/setup boundary.  A later admitted ordinary update must strictly pass
that deadline before cut preparation, and rendering still requires separate
camera, state/view, device and backend admission.
