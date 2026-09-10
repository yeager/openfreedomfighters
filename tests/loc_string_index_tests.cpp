#include "off/data/loc_string_index.hpp"

#include <array>
#include <cstddef>
#include <iostream>

int main() {
  constexpr std::array<std::byte, 15> fixture{
      std::byte{1}, std::byte{'K'}, std::byte{'e'}, std::byte{'y'}, std::byte{},
      std::byte{2}, std::byte{'t'}, std::byte{'e'}, std::byte{'x'}, std::byte{'t'},
      std::byte{}, std::byte{0x7f}, std::byte{'x'}, std::byte{3}, std::byte{}};
  const auto index = off::data::LocStringIndex::scan(fixture);
  if (index.candidates().size() != 2 || index.candidates()[0].offset != 1 ||
      index.candidates()[0].bytes != "Key" || index.candidates()[1].offset != 6 ||
      index.candidates()[1].bytes != "text") {
    std::cerr << "LOC candidate scan did not retain bounded printable NUL runs\n";
    return 1;
  }
  constexpr std::array<std::byte, 2> unterminated{std::byte{'x'}, std::byte{'y'}};
  if (!off::data::LocStringIndex::scan(unterminated).candidates().empty()) {
    std::cerr << "LOC candidate scan accepted an unterminated run\n";
    return 1;
  }
  return 0;
}
