# Private Windows observation: intro and first gameplay frame

This note records behavior-only conclusions from a private, user-authorized
Windows observation on 2026-09-09. The captures, their text, and all extracted
pixels remain private. This document does not distribute screenshots, dialogue,
retail fonts, or retail image data.

## What the observation establishes

- The executable reaches a cinematic introduction before the first controllable
  gameplay frame.
- Early cinematic frames use a dark background with a left-aligned textual
  information block in the lower-left region. The text is not retained here.
- A later cinematic frame contains the game title treatment over a scene image.
  This confirms a multi-frame presentation sequence rather than a single static
  legal card.
- The first observed gameplay frame has HUD elements in the upper-left and
  upper-right regions plus a left-side movement tutorial prompt. The camera is
  a downward-looking third-person view in an interior environment.

## Private x86 Steam observation

A separate, user-authorized Steam run on x86 Linux in September 2026 reached
the retail launcher, its explicit play action, and the original opening
sequence. The compatibility layer and captures remain private and are not a
native target qualification.

This repeat establishes that the executable passes through a launcher boundary
before the game process and that the opening sequence includes chronology-card
presentation before a title treatment. The observed frames used a cinematic
letterboxed presentation rather than the full-height gameplay viewport. It
does not retain card wording, images, frame timings, audio, input behavior, or
any executable-derived details.

This corroborates the broad opening-state ordering recorded above. It does not
authorize a pixel replica, timeline implementation, or automatic native intro
presentation.

A private capture-set audit retained five mutually distinct full-window samples
at one 16:9 capture extent. It corroborates that the observed opening changes
visual state before gameplay; it does not establish a scene count, an authored
viewport, frame durations, transition semantics, or a playback order beyond
the separately observed broad chronology.

`tools/intro_visual_timeline_audit.py` is the private receipt boundary for a
future repeat. An external observer compares its own isolated capture set and
provides only a bounded order plus the categorical relation to the previous
sample: initial, unchanged, or changed. The tool rejects all other fields and
retains only the capture extent, count, and orders at which a visual change was
observed. It does not open, copy, hash, or retain screenshots; it does not
launch, attach to, or control an original process. Input and output must remain
outside the repository, and an existing receipt is never overwritten.

## What it does not establish

The sample does not identify the serialized sequence-player records, frame
durations, transitions, camera matrices, text source, skip input, audio cue
mapping, blending, viewport policy, or the original renderer's aspect-ratio
behavior. It therefore does not authorize an automatic intro draw path or a
replacement menu layout.

## Implementation consequence

`IntroRuntime` may continue to retain and validate source-backed picture and
audio resources, but normal startup must keep automatic presentation disabled
until the sequence lifecycle and an admitted camera/view are recovered. The
existing SDL intro renderer and presentation adapters remain useful native
building blocks; they are not evidence that the observed sequence is playing.

The first gameplay frame confirms that later validation must cover both
cinematic presentation and HUD/world rendering. It does not make the diagnostic
scene renderer a gameplay implementation.
