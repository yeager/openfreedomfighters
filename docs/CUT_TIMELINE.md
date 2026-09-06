# Cut timeline conversion and execution boundary

The native `off::cutscene::timeline_position` function converts an explicit
current/start scene-clock word pair to the cut player's timeline coordinate.
It does not sample a host clock, select a rate, activate a player or dispatch
commands. The [scene-clock evidence](TIMING_EVIDENCE.md) and
[intro bootstrap gates](INTRO_BOOTSTRAP.md) remain separate.

## Reviewed conversion

1. Subtract the start word from the current word using 32-bit unsigned arithmetic.
2. Interpret the resulting bits as a signed 32-bit value.
3. Multiply by 25600 in signed 64-bit arithmetic and arithmetic-shift right ten.
4. Retain the low 32 bits and interpret them as signed.
5. Convert to float, then multiply by exactly 1/1024 at float precision.

The product fits signed 64-bit range. In this particular expression, 25600 is
divisible by 1024, so the shift loses no fractional information. The subsequent
low-word retention and float conversion are observable and must not be replaced
by an unbounded double-precision elapsed-time formula. Deliberate bit operations
avoid C++ signed-overflow undefined behavior. They reproduce this conversion;
they do not establish overflow policy for the upstream engine clock.

Tests use explicit numerical inputs, not invented retail captures or wall-clock
timings. Boundary vectors cover zero, negative differences, subtraction wrap,
signed extrema, multiplication's retained low word and integer-to-float rounding.
The 36-test suite and targeted converter ASan/UBSan run pass locally. These checks
validate arithmetic, not an original-clock capture or rendered intro.

## Execution ordering still under research

The recovered active-update phases visit member starts, then member ends, then
overdue commands. Member activation/deactivation returns before the corresponding
fired flag is stored. A command executes before advancing its cursor. Both member
boundaries and command schedules use strict greater-than comparisons.

Cleanup is checked after those phases. An end request made by an earlier callback
can therefore affect the same update. The subsequent end-condition calculation
can set a new end request without immediately invoking cleanup. Start and cleanup
also have post-callback state writes. These facts do not justify transactional,
deferred or non-reentrant behavior without further evidence.

Command insertion is not a proven stable sort: the observed ordered container
places equal keys before its selected equal candidate, while actual lifecycle
registration order remains unresolved. Parsed attachment order must be preserved
as data, but must not automatically become a claimed execution order. Complete
cursor behavior, admission branches, callback delivery and resource
activation still block a faithful complete player. No 60 Hz queue or splash timer
is substituted for those contracts.
