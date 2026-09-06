# Incremental intro audio input

`IntroPreparedAudio::open_stream()` opens the bank selected by the retained
SND/WHD binding. The resulting move-only stream owns its file reader, encoded
storage and persistent Vorbis decoder. It does not prepare a sound record or
open an output device.

The initial request reads two 32-KiB windows on a worker. Short sources use their
actual encoded length as a native policy; the two owned intro streams both exceed
64 KiB. Issuing the read, receiving its completion, and observing completion are
separate operations. Only the last makes encoded input ready. I/O errors cannot
publish a successful completion.

After observation, callers request bounded PCM chunks. Opening the decoder and
pulling additional encoded bytes happen on the worker, through nonseekable
callbacks. Later input refills use 32-KiB chunks. The decoder retains its state
between requests rather than decoding the complete song again. Each PCM result
owns interleaved signed 16-bit samples. EOF must agree with the WHD meaningful
sample count; corrupt, chained or inconsistent sources fail explicitly.

This uses Xiph's [callback-based opening](https://xiph.org/vorbis/doc/vorbisfile/ov_open_callbacks.html)
and [incremental PCM reads](https://xiph.org/vorbis/doc/vorbisfile/ov_read.html).
The same decoder object is retained across requests, and callback exceptions
are captured inside the C boundary before being reported on the controlling thread.

## Lifetime and error policy

One controlling thread owns the stream API. Work never accesses canonical sound
records. Cancellation suppresses completion publication; it does not make a
blocked filesystem read disappear. Destruction and move assignment drain pending
work before freeing encoded storage or the decoder. Moving a pending stream
preserves its heap-stable worker context. A cancelled stream is not reusable.

Control operations do not wait for worker I/O, but may allocate or create a
thread; they are not real-time-safe audio callbacks. File opening and teardown
can block. This implementation uses one asynchronous job per request, not a
finished mixer worker pool or original refill scheduler.

The retained VFS reader checks the size of the opened file, bounds every read,
and fails on short input. Reads keep using that opened file when its path is
replaced. It is not an immutable snapshot: in-place writes and the pre-open
path-check race are not eliminated. Installation files must remain unchanged
after startup verification.

## What this does not do

Encoded readiness and populated PCM are prerequisites, not playback-start
acknowledgements. No stream method changes sound-record progress, sends
`SoundReady`, or bypasses channel admission. Original channel sizing, command
pumping, output-buffer acquire/start and component event dispatch remain work
for the retained runtime. Normal startup does not automatically start these
streams yet.

Independent tests exercise mono/stereo streams, multiple refills, exact PCM
agreement with the offline decoder, count errors, truncated input, chained
streams, cancellation and moving pending work. Owned-data probes use the same
public entry point without saving audio into the repository. PCM agreement does
not establish the original engine's refill timing or audible output.
