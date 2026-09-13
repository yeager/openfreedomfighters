# Intro reader-frontier evidence receipt

This workflow records that a private, user-owned cold probe reached the
reader-frontier audit. It is an implementation-research receipt, not reader
admission evidence. It makes no claim about a payload grammar, destination,
lifecycle completion, rendering, activation, audio, or playback.

Capture the probe transcript in a new private file outside the checkout, then
create a new receipt in another private location:

```sh
openfreedomfighters --probe-first-cut-cold --data /private/owned-data > /private/cold-probe.txt
python3 tools/intro_reader_frontier_receipt.py \
  /private/cold-probe.txt /private/reader-frontier-receipt.json
```

The tool refuses repository paths, symlinks, non-regular or oversized input,
and an existing receipt. It reads the transcript only to confirm completion and
well-formed aggregate frontier records. It neither prints, copies, hashes, nor
retains any transcript value.

The receipt has only five categorical fields: format, probe kind, frontier
observed, reader admission unchanged, and playback not started. In particular,
it contains no retail paths, IDs, offsets, bytes, text, checksums, source
labels, images, or aggregate counts. Keep both the transcript and receipt
private; never add either to the repository or an issue tracker.

For the frontier's permitted aggregate research interpretation and the evidence
required before any reader work, see
[INTRO_DEFERRED_READER_COVERAGE.md](INTRO_DEFERRED_READER_COVERAGE.md).
