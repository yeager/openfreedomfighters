#pragma once

#include "off/graphics/intro_named_global_section_envelope.hpp"

#include <array>
#include <string>
#include <string_view>

namespace off::graphics {

// Native comparison for the reviewed ASCII scene-name domain. Bytes outside
// ASCII are compared unchanged; this is not locale-dependent case folding.
struct IntroPropertyNameLess final {
  using is_transparent = void;

  [[nodiscard]] bool operator()(std::string_view left,
                                std::string_view right) const noexcept {
    const auto fold = [](unsigned char value) {
      return value >= 'A' && value <= 'Z'
                 ? static_cast<unsigned char>(value + ('a' - 'A'))
                 : value;
    };
    const auto common = left.size() < right.size() ? left.size() : right.size();
    for (std::size_t index = 0; index < common; ++index) {
      const auto a = fold(static_cast<unsigned char>(left[index]));
      const auto b = fold(static_cast<unsigned char>(right[index]));
      if (a != b) return a < b;
    }
    return left.size() < right.size();
  }
};

// The supported intro form has exactly two named scalar null references.
// Names are copied from owned input, never compiled from retail text. The
// scalar registry type is 0x10, not an integer or a live native resource ID.
struct IntroNamedGlobalNullReferences final {
  std::array<std::string, 2> names;
  static constexpr std::uint32_t registry_type = 0x10U;
  static constexpr std::uint32_t reference_bits = 0U;
};

// The null-only relocation pass makes no reference lookup and changes no
// bytes. It still validates the complete supported reference grammar.
inline void validate_intro_named_global_null_reference_block(
    std::span<const std::byte> block) {
  if (block.size() != 16U || block[0] != std::byte{16} ||
      block[1] != std::byte{} || block[2] != std::byte{} || block[3] != std::byte{})
    throw std::runtime_error("Intro named-reference block framing is unsupported");
  const auto pair = read_intro_named_global_word_pair({{}, block});
  if (pair.first_tag != 0x08U || pair.second_tag != 0x08U ||
      pair.first_word != 0U || pair.second_word != 0U)
    throw std::runtime_error("Intro named-reference type or nonzero relocation is unsupported");
}

[[nodiscard]] inline IntroNamedGlobalNullReferences
read_intro_named_global_null_references(
    const IntroNamedGlobalSectionEnvelope& envelope) {
  constexpr std::size_t max_name_bytes = 1024U;
  if (envelope.label.empty() || envelope.label.size() > 2U * max_name_bytes + 1U)
    throw std::runtime_error("Intro named-reference label exceeds supported limits");
  const auto comma = envelope.label.find(',');
  if (comma == std::string_view::npos || comma == 0U ||
      comma + 1U == envelope.label.size() ||
      envelope.label.find(',', comma + 1U) != std::string_view::npos)
    throw std::runtime_error("Intro named-reference source requires two nonempty names");
  const std::array<std::string_view, 2> names{
      envelope.label.substr(0U, comma), envelope.label.substr(comma + 1U)};
  for (const auto name : names) {
    if (name.size() > max_name_bytes)
      throw std::runtime_error("Intro named-reference name exceeds supported limits");
    for (const unsigned char byte : name)
      if (byte < 0x20U || byte > 0x7eU)
        throw std::runtime_error("Intro named-reference names require printable ASCII");
  }
  const IntroPropertyNameLess less;
  if (!less(names[0], names[1]) && !less(names[1], names[0]))
    throw std::runtime_error("Intro named-reference names must be distinct");

  // Validate the complete supported framing before exposing either entry.
  // This suffix check is native validation, not a claim that the original
  // name-driven reader consumed the delimiter/terminator after its last value.
  validate_intro_named_global_null_reference_block(envelope.tagged_block);
  return {{std::string(names[0]), std::string(names[1])}};
}

} // namespace off::graphics
