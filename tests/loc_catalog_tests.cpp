#include "off/data/loc_catalog.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

void check(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}

void text(std::vector<std::byte>& bytes, std::string_view value) {
  for (const auto character : value) bytes.push_back(static_cast<std::byte>(character));
  bytes.push_back(std::byte{});
}

void u32(std::vector<std::byte>& bytes, std::uint32_t value) {
  for (unsigned shift{}; shift < 32U; shift += 8U)
    bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}

std::vector<std::byte> scalar_node(std::string_view key, std::string_view value) {
  std::vector<std::byte> result;
  text(result, key);
  result.push_back(std::byte{1});
  text(result, value);
  return result;
}

std::vector<std::byte> multi_node(std::string_view key, std::string_view first,
                                  std::string_view second) {
  std::vector<std::byte> result;
  text(result, key);
  result.push_back(std::byte{3});
  text(result, first);
  text(result, second);
  return result;
}

std::vector<std::byte> list(std::vector<std::vector<std::byte>> children) {
  std::vector<std::byte> result;
  result.push_back(static_cast<std::byte>(children.size()));
  std::uint32_t boundary{};
  for (std::size_t index{}; index + 1U < children.size(); ++index) {
    boundary += static_cast<std::uint32_t>(children[index].size());
    u32(result, boundary);
  }
  for (const auto& child : children) result.insert(result.end(), child.begin(), child.end());
  return result;
}

std::vector<std::byte> named_list_node(std::string_view key,
                                       std::vector<std::vector<std::byte>> children) {
  std::vector<std::byte> result;
  text(result, key);
  const auto body = list(std::move(children));
  result.insert(result.end(), body.begin(), body.end());
  return result;
}

}  // namespace

int main() {
  try {
    const auto nested = named_list_node(
        "synthetic.group",
        {scalar_node("synthetic.first", "Project authored first"),
         scalar_node("synthetic.second", "Project authored second")});
    const auto member = list({scalar_node("synthetic.root", "Project authored root"),
                              nested,
                              multi_node("synthetic.format", "Project authored A",
                                         "Project authored B")});
    const auto decoded = off::data::decode_loc_member_display_texts(member);
    check(decoded && decoded->values == std::vector<std::string>{
              "Project authored root", "Project authored first", "Project authored second",
              "Project authored A", "Project authored B"},
          "decoder preserves depth-first child and tail order");

    const auto empty_value_member = list({
        scalar_node("synthetic.blank", ""),
        scalar_node("synthetic.nonblank", "Project authored nonblank")});
    const auto empty_values = off::data::decode_loc_member_display_texts(empty_value_member);
    check(empty_values && empty_values->values == std::vector<std::string>{
              "", "Project authored nonblank"},
          "decoder preserves intentionally blank display-text values in canonical order");

    auto bad_offset = member;
    bad_offset[1] = std::byte{2};
    bad_offset[2] = std::byte{0xff};
    bad_offset[3] = std::byte{0xff};
    bad_offset[4] = std::byte{0xff};
    bad_offset[5] = std::byte{0x7f};
    check(!off::data::decode_loc_member_display_texts(bad_offset),
          "out-of-range child boundaries are rejected without fallback");

    const std::vector<std::byte> bad_utf8{
        std::byte{1}, std::byte{'k'}, std::byte{}, std::byte{1}, std::byte{0xc3}, std::byte{}};
    check(!off::data::decode_loc_member_display_texts(bad_utf8),
          "invalid source UTF-8 is rejected rather than decoded as a legacy code page");

    const std::vector<std::byte> incomplete = list({
        {std::byte{'k'}, std::byte{}}, scalar_node("synthetic.ok", "Project authored")});
    check(!off::data::decode_loc_member_display_texts(incomplete),
          "named nodes without a complete record are rejected");

    std::cout << "LOC catalog tests passed\n";
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
