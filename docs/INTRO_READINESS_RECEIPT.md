# Intro readiness evidence receipt

This private workflow records the aggregate reader and lifecycle coverage from
a completed, cold intro-readiness probe. It is an implementation-boundary
receipt, not evidence that an intro can start, render, play audio, or run a
lifecycle.

Capture the probe transcript in a new private file outside the checkout, then
create a new receipt in another private location:

```sh
openfreedomfighters --probe-intro-readiness --data /private/owned-data > /private/intro-readiness.txt
python3 tools/intro_readiness_receipt.py \
  /private/intro-readiness.txt /private/intro-readiness-receipt.json
```

The receipt validates the four reader totals, the required/covered/uncovered
totals for readers, components, and owners, their fail-closed lifecycle status,
and the explicit cold statuses: lifecycle not admitted, renderer not created,
audio not started, and playback not started. It rejects missing, duplicate, or
arithmetically inconsistent records.

The tool refuses repository paths, symlinks, non-regular or oversized input,
and an existing receipt. It retains only the source-free aggregate schema; it
never copies source identities, paths, labels, offsets, handles, payloads, or
transcript text. Keep the transcript and receipt private and do not add either
to the repository or an issue tracker.
