# Graphics settings overlay

The graphics settings overlay is a portable presentation feature. It exposes
requested settings separately from capabilities detected at runtime, and keeps
the application usable when an optional graphics feature is unavailable.

## Interaction and transaction contract

- F10 opens and closes the overlay; Escape cancels an uncommitted draft.
- Keyboard and controller navigation remain usable without pointer input.
- Disabled options explain why they cannot be applied.
- Applying settings validates the full draft and prepares resources before
  changing live state. Failure restores the last known-good configuration.
- Risky display changes require confirmation. Persist only settings the user
  confirms; a rejected or expired change is not saved.
- UI rendering stays at output resolution when scene rendering is scaled.

The platform-neutral session owns draft, confirmation, and rollback state. SDL
translates events and applies platform changes. Persistence stores requested
intent; detected capabilities and fallback reasons are recomputed at launch.

## Optional rendering features

DLSS, FSR, XeSS, temporal upscaling, and other optional features may be shown as
requested settings only when the application can report whether a compatible
adapter is active. A label must identify the implementation actually loaded.
An unavailable feature resolves to a documented fallback and a visible reason;
it must never claim that a different filter is a vendor feature.

Graphics settings do not change authoritative simulation state, input timing,
random state, saves, or replay results. Original and Modern presentation modes
consume the same simulation output.

## Accessibility and platform scope

Keyboard-only and controller-only operation, visible focus, scalable text,
non-color status cues, safe-area padding, and reduced-motion behavior are
requirements. Pointer controls are supplemental. Platform accessibility API
integration is not claimed until validated on each target.

Windows, Linux, Steam Deck, and macOS must retain portable settings and a usable
fallback path. Backend-specific features are enabled only when that backend and
its optional runtime are supported.

## Verification

Public tests should use project-authored settings and synthetic capabilities.
Hardware smoke checks may validate display changes and optional adapters locally;
any captures containing user installation content must remain outside the source
repository and release artifacts.
