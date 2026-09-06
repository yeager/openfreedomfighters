#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace off::audio {
// Concrete nonspatial signed-16 stereo output boundary. Implementations report
// failures by exception. Successful start admits playback but never publishes an
// acknowledgement; the channel manager owns that event.
class StereoPcmOutput {
public:
  virtual ~StereoPcmOutput() = default;
  virtual void submit(std::span<const std::int16_t> samples) = 0;
  [[nodiscard]] virtual std::size_t queued_input_bytes() const = 0;
  virtual void start() = 0;
  virtual void pause() = 0;
  virtual void stop() = 0;
  virtual void flush() = 0;
  virtual void set_volume_hundredths_db(std::int32_t volume) = 0;
  virtual void set_frequency(std::uint32_t frequency_hz) = 0;
  virtual void set_pan(std::int32_t pan) = 0;
};
} // namespace off::audio
