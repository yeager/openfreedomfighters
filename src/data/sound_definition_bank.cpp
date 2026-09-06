#include "off/data/sound_definition_bank.hpp"
#include "off/data/byte_reader.hpp"
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace off::data {

SoundDefinitionBank SoundDefinitionBank::parse(
    std::span<const std::byte> image, std::size_t byte_budget) {
  if (image.size() > byte_budget || image.size() > std::numeric_limits<std::uint32_t>::max())
    throw std::runtime_error("Sound definition bank exceeds the native byte budget");
  SoundDefinitionBank result;
  result.image_.assign(image.begin(), image.end());
  return result;
}

std::optional<SimpleSoundDefinition> SoundDefinitionBank::simple_definition(
    std::uint32_t reference, std::size_t identifier_byte_limit) const {
  if (reference == 0) return std::nullopt;
  const ByteReader reader(image_);
  // Validate the whole fixed record before accessing any of its fields.
  const ByteReader record(reader.slice(reference, 16));
  if (record.u32(0) != 1)
    throw std::runtime_error("Unsupported sound definition record type");
  const auto identifier_offset = record.u32(4);
  const auto resource_link = record.u32(8);
  const auto duration_bits = record.u32(12);
  const float duration = std::bit_cast<float>(duration_bits);
  if (!std::isfinite(duration) || duration < 0)
    throw std::runtime_error("Sound definition duration must be finite and nonnegative");
  if (identifier_offset >= image_.size())
    throw std::runtime_error("Sound identifier offset exceeds its bank");
  std::size_t end = identifier_offset;
  while (end < image_.size() && image_[end] != std::byte{0}) {
    if (end - identifier_offset >= identifier_byte_limit)
      throw std::runtime_error("Sound identifier exceeds the native byte limit");
    ++end;
  }
  if (end == image_.size())
    throw std::runtime_error("Sound identifier is not terminated within its bank");
  std::string name;
  name.reserve(end - identifier_offset);
  for (std::size_t index = identifier_offset; index < end; ++index)
    name.push_back(static_cast<char>(std::to_integer<unsigned char>(image_[index])));
  return SimpleSoundDefinition{reference, identifier_offset, resource_link,
                              duration_bits, duration, std::move(name)};
}

} // namespace off::data
