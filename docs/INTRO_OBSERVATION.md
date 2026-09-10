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
