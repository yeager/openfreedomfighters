# Incremental intro audio input

`IntroPreparedAudio::open_stream()` opens a prepared bank reader and returns a
move-only stream that owns its reader, encoded storage, and persistent decoder.
It does not prepare canonical sound records or open an output device.

The stream reads encoded data incrementally and exposes bounded PCM requests.
Decoder state persists between requests. Read completion, observation of that
completion, and decoded output are distinct states; errors must not be reported
as successful readiness. Each PCM result owns its sample storage.

The implementation uses Xiph's [callback-based opening](https://xiph.org/vorbis/doc/vorbisfile/ov_open_callbacks.html)
and [incremental PCM reads](https://xiph.org/vorbis/doc/vorbisfile/ov_read.html).
Exceptions are contained at the C callback boundary and reported on the
controlling thread.

## Lifetime and error policy

One controlling thread owns the stream API. Cancellation suppresses completion
publication but cannot cancel a blocked filesystem operation. Destruction and
move assignment drain pending work before releasing storage. Control operations
may allocate or create a thread and are not real-time-safe callbacks.

The retained VFS reader bounds reads and validates the opened handle. It detects
short reads and file-size changes; in-place writes and the pre-open path-check
race are outside this guarantee. Installation files must remain unchanged after
startup verification.

## Scope and verification

Encoded readiness and populated PCM do not acknowledge playback start. Stream
methods do not alter canonical sound-record state or bypass channel admission.
Tests use generated audio fixtures to cover incremental reads, cancellation,
moves, malformed input, and decode failures. Do not publish installation audio,
its lengths, or analysis results. Decoding success does not establish audible
output or complete startup playback.
