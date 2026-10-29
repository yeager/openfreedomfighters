#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace off::data {
// A deliberately narrow, independently validated profile. A successful parse
// says nothing about script execution or a native relocation receipt.
struct PostloadIntegerEntry {
  std::string name;
  std::uint32_t bits{};
};
struct PostloadIntegerProfile {
  std::string source_text;
  std::vector<PostloadIntegerEntry> entries;
};
[[nodiscard]] std::optional<PostloadIntegerProfile>
parse_postload_integer_profile(std::span<const std::byte> frame);
}
