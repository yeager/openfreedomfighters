#include "off/data/animation_image.hpp"
#include "off/data/zip_archive.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

void set_u32(std::vector<std::byte> &bytes, std::size_t offset,
             std::uint32_t value) {
  for (unsigned shift = 0; shift < 32U; shift += 8U) {
    bytes[offset + shift / 8U] =
        static_cast<std::byte>((value >> shift) & 0xffU);
  }
}

std::vector<std::byte> fixture() {
  std::vector<std::byte> bytes(232U, std::byte{0});
  set_u32(bytes, 0, 0x00414e4dU);
  set_u32(bytes, 4, 0x80000000U | static_cast<std::uint32_t>(bytes.size()));
  set_u32(bytes, 8, static_cast<std::uint32_t>(bytes.size()));
  set_u32(bytes, 12, 10U);
  set_u32(bytes, 16, 10U);
  set_u32(bytes, 20, 0x80000024U);
  set_u32(bytes, 24, 28U);
  set_u32(bytes, 28, 1U);
  set_u32(bytes, 32, 1U);
  set_u32(bytes, 36, 12U);
  set_u32(bytes, 40, 100U);
  const char name[] = "A.anm";
  for (std::size_t index = 0; index < sizeof(name) - 1U; ++index)
    bytes[44U + index] = static_cast<std::byte>(name[index]);
  set_u32(bytes, 52, 14U);
  set_u32(bytes, 56, 44U);
  set_u32(bytes, 64, 2U);
  set_u32(bytes, 72, 0U);
  set_u32(bytes, 76, 4U);
  set_u32(bytes, 80, 1U);
  set_u32(bytes, 84, 1U);
  set_u32(bytes, 88, 6U);
  set_u32(bytes, 96, 7U);
  return bytes;
}

template <typename Mutation>
void check_rejected(Mutation mutate, const char *message) {
  auto bytes = fixture();
  mutate(bytes);
  try {
    static_cast<void>(off::data::AnimationImage::parse(bytes));
    check(false, message);
  } catch (const std::runtime_error &) {
  }
}

} // namespace

int main(int argc, char **argv) {
  const auto bytes = fixture();
  const auto image = off::data::AnimationImage::parse(bytes);
  check(image.header().byte_size == bytes.size(), "retain animation byte size");
  check(image.header().reference_table_count == 1U &&
            image.header().format_value == 10U,
        "retain animation reference-table count and format value");
  check(image.reference_tables().size() == 1U &&
            image.reference_tables()[0].name == "A.anm" &&
            image.reference_tables()[0].reference_words ==
                std::vector<std::uint32_t>{100U},
        "decode the animation reference table directory");
  check(image.descriptors().size() == 3U &&
            image.descriptors()[0].opaque_word_0 == 2U &&
            image.descriptors()[1].tag == 1U &&
            image.descriptors()[2].tag == 7U,
        "decode the animation descriptor block");
  check_rejected([](auto &value) { set_u32(value, 0, 0); },
                 "reject wrong animation signature");
  check_rejected([](auto &value) { set_u32(value, 4, 60U); },
                 "reject unflagged animation root size");
  check_rejected([](auto &value) { set_u32(value, 8, 56U); },
                 "reject declared-size mismatch");
  check_rejected([](auto &value) { set_u32(value, 12, 9U); },
                 "reject zero animation reference tables");
  check_rejected([](auto &value) { set_u32(value, 12, 16U); },
                 "reject too many animation reference tables");
  check_rejected([](auto &value) { set_u32(value, 16, 11U); },
                 "reject unsupported animation format value");
  check_rejected([](auto &value) { set_u32(value, 40, 102U); },
                 "reject a misaligned animation reference word");
  check_rejected([](auto &value) { set_u32(value, 20, 0x80000028U); },
                 "reject excess animation reference table padding");
  check_rejected([](auto &value) { set_u32(value, 24, 32U); },
                 "reject an invalid animation reference table base length");
  check_rejected([](auto &value) { set_u32(value, 36, 16U); },
                 "reject an invalid animation reference table name offset");
  check_rejected([](auto &value) { set_u32(value, 52, 10U); },
                 "reject a non-final marker on the final table");
  check_rejected([](auto &value) { set_u32(value, 60, 1U); },
                 "reject a nonzero animation descriptor reserved word");
  check_rejected([](auto &value) { set_u32(value, 56, 45U); },
                 "reject a misaligned animation descriptor block length");
  check_rejected([](auto &value) { set_u32(value, 72, 7U); },
                 "reject an early animation descriptor terminator");
  check_rejected([](auto &value) { set_u32(value, 96, 5U); },
                 "reject a missing animation descriptor terminator");
  auto truncated = fixture();
  truncated.resize(19U);
  try {
    static_cast<void>(off::data::AnimationImage::parse(truncated));
    check(false, "reject truncated animation header");
  } catch (const std::runtime_error &) {
  }
  if (argc == 2) {
    std::size_t parsed_files = 0;
    std::size_t parsed_tables = 0;
    std::size_t parsed_descriptors = 0;
    for (const auto &item :
         std::filesystem::recursive_directory_iterator(argv[1])) {
      if (!item.is_regular_file())
        continue;
      auto archive_extension = item.path().extension().string();
      std::ranges::transform(
          archive_extension, archive_extension.begin(),
          [](unsigned char value) { return std::tolower(value); });
      if (archive_extension != ".zip")
        continue;
      const auto archive = off::data::ZipArchive::open(item.path());
      for (const auto &entry : archive.entries()) {
        auto member_extension =
            std::filesystem::path(entry.name).extension().string();
        std::ranges::transform(
            member_extension, member_extension.begin(),
            [](unsigned char value) { return std::tolower(value); });
        if (member_extension != ".anm")
          continue;
        const auto parsed =
            off::data::AnimationImage::parse(archive.read(entry));
        ++parsed_files;
        parsed_tables += parsed.reference_tables().size();
        parsed_descriptors += parsed.descriptors().size();
      }
    }
    check(parsed_files == 42U && parsed_tables == 112U &&
              parsed_descriptors == 457U,
          "parse the complete supported animation corpus");
    if (failures == 0)
      std::cout << "Validated " << parsed_files << " ANM files, "
                << parsed_tables << " reference tables, and "
                << parsed_descriptors << " descriptors\n";
  }
  return failures == 0 ? 0 : 1;
}
