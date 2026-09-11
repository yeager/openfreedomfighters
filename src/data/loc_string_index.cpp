#include "off/data/loc_string_index.hpp"

#include <cstdint>
#include <algorithm>
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
[[nodiscard]] bool ascii_identifier_like(std::span<const std::byte> bytes) noexcept {
  if (bytes.empty()) return false;
  for (const auto value : bytes) {
    const auto byte = static_cast<unsigned char>(value);
    const bool alpha = (byte >= 'A' && byte <= 'Z') ||
                       (byte >= 'a' && byte <= 'z');
    const bool digit = byte >= '0' && byte <= '9';
    if (!alpha && !digit && byte != '_') return false;
  }
  return true;
}

void hash_u64(std::uint64_t value, std::uint64_t& digest) noexcept {
  for (unsigned byte{}; byte < sizeof(value); ++byte) {
    digest ^= (value >> (byte * 8U)) & 0xffU;
    digest *= 1099511628211ULL;
  }
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
    result.candidates_.push_back({start, std::string_view(text, cursor - start),
                                  ascii_identifier_like(bytes.subspan(start, cursor - start))});
    ++cursor;
  }
  return result;
}

LocStringProfile LocStringIndex::profile(std::span<const std::byte> bytes) {
  const auto index = scan(bytes);
  LocStringProfile result{.member_bytes = bytes.size()};
  for (const auto& candidate : index.candidates()) {
    const auto length = candidate.bytes.size();
    if (length > std::numeric_limits<std::size_t>::max() - result.candidate_bytes)
      throw std::runtime_error("LOC candidate bytes exceed the safety limit");
    ++result.candidate_count;
    result.ascii_identifier_candidate_count += candidate.ascii_identifier_like ? 1U : 0U;
    result.candidate_bytes += length;
    result.maximum_candidate_bytes = std::max(result.maximum_candidate_bytes, length);
    hash_u64(candidate.offset, result.structure_digest);
    hash_u64(length, result.structure_digest);
    hash_u64(candidate.ascii_identifier_like ? 1U : 0U, result.structure_digest);
  }
  return result;
}

}  // namespace off::data
