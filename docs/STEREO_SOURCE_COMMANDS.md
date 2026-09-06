# Intro stereo source commands

The logical backend retains master gain separately from category gains. Sound
records retain pan, producer-written priority and grouping state. Frequency
adjustment uses the second timing-change integer; grouping count replaces the
provisional repetition-count name. Unassigned priority and environment indices
remain optional.

The explicit builder consumes an already admitted, priority-ordered binding
list and visits at most 65 entries, including skipped bindings. Simple SND
links produce nonspatial commands. State 5 becomes state 1 with a start request;
muted records still produce commands. Missing SND/WHD references produce
diagnostics, not guessed bank rows or playback acknowledgements.

Gain products have separate binary32 rounding steps. Output volume conversion
is logarithmic, distinct from the category response curve. Pan clamps to
[-10000,10000]; frequency uses signed adjustment arithmetic and integer
clamping to [100,100000] Hz. Nonfinite arithmetic and unrepresentable signed
conversions fail explicitly. Native transcendental math is an interoperability
policy, not a claim of bit-exact original execution.

Synthetic tests cover field mapping, start-state changes, muted sources, visit
limits, missing references, grouping, gain conversion and arithmetic bounds.
They do not demonstrate audible playback. Normal startup does not yet call
the builder: listener/grouping admission, complete command batches, channel
allocation, incremental refill and real acknowledgement delivery remain work
to integrate. No source command by itself acknowledges playback.
