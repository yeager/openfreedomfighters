#include "off/data/loc_string_index.hpp"
#include "off/data/zip_archive.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>

int main() {
  constexpr std::array<std::byte, 17> fixture{
      std::byte{1}, std::byte{'K'}, std::byte{'e'}, std::byte{'y'}, std::byte{},
      std::byte{2}, std::byte{'t'}, std::byte{'e'}, std::byte{'x'}, std::byte{'t'},
      std::byte{}, std::byte{0x7f}, std::byte{'x'}, std::byte{3}, std::byte{},
      std::byte{0xe5}, std::byte{}};
  const auto index = off::data::LocStringIndex::scan(fixture);
  if (index.candidates().size() != 3 || index.candidates()[0].offset != 1 ||
      index.candidates()[0].bytes != "Key" || !index.candidates()[0].ascii_identifier_like ||
      index.candidates()[1].offset != 6 || index.candidates()[1].bytes != "text" ||
      !index.candidates()[1].ascii_identifier_like || index.candidates()[2].offset != 15 ||
      index.candidates()[2].bytes.size() != 1 ||
      static_cast<unsigned char>(index.candidates()[2].bytes.front()) != 0xe5U ||
      index.candidates()[2].ascii_identifier_like) {
    std::cerr << "LOC candidate scan did not retain bounded printable NUL runs\n";
    return 1;
  }
  constexpr std::array<std::byte, 2> unterminated{std::byte{'x'}, std::byte{'y'}};
  if (!off::data::LocStringIndex::scan(unterminated).candidates().empty()) {
    std::cerr << "LOC candidate scan accepted an unterminated run\n";
    return 1;
  }
  // CI has no retail data. An owner may opt in to validate the scanner against
  // their local archive without copying its bytes into this repository.
  if (const auto* root = std::getenv("OFF_LOC_DATA_ROOT"); root != nullptr && *root != '\0') {
    const auto archive = off::data::ZipArchive::open(
        std::filesystem::path(root) / "Scenes" / "FF-StartUp.ZIP");
    const auto* entry = archive.find("SCENES/FF-StartUp.LOC");
    if (entry == nullptr || off::data::LocStringIndex::scan(archive.read(*entry)).candidates().empty()) {
      std::cerr << "owned LOC candidate scan failed\n";
      return 1;
    }
  }
  return 0;
}
