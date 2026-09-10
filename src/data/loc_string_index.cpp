#include "off/data/loc_string_index.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace off::data {
namespace {
constexpr std::size_t maximum_candidate_count = 1'000'000;
[[nodiscard]] bool printable(std::byte value) noexcept {
  const auto byte = static_cast<unsigned char>(value);
  // LOC encoding has not yet been recovered. Retain every non-control octet
  // as raw data so an eventual legacy-code-page or UTF-8 decoder receives the
  // exact source span rather than an ASCII-filtered approximation.
  return byte >= 0x20U && byte != 0x7fU;
}
}  // namespace

LocStringIndex LocStringIndex::scan(std::span<const std::byte> bytes) {
  if (bytes.size() > std::numeric_limits<std::uint32_t>::max())
    throw std::runtime_error("LOC member exceeds the supported size");
  LocStringIndex result;
  for (std::size_t cursor{}; cursor < bytes.size();) {
    if (!printable(bytes[cursor])) { ++cursor; continue; }
    const auto start = cursor;
    while (cursor < bytes.size() && printable(bytes[cursor])) ++cursor;
    if (cursor == bytes.size() || bytes[cursor] != std::byte{}) continue;
    if (result.candidates_.size() == maximum_candidate_count)
      throw std::runtime_error("LOC candidate count exceeds the safety limit");
    const auto* text = reinterpret_cast<const char*>(bytes.data() + start);
    result.candidates_.push_back({start, std::string_view(text, cursor - start)});
    ++cursor;
  }
  return result;
}

}  // namespace off::data
