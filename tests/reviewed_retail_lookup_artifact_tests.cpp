#include "off/ui/reviewed_retail_lookup_artifact.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void append_u64(std::string &output, std::uint64_t value) {
  for (unsigned byte{}; byte < 8U; ++byte)
    output.push_back(static_cast<char>((value >> (byte * 8U)) & 0xffU));
}
void append_blob(std::string &output, std::string_view value) {
  append_u64(output, value.size());
  output.append(value);
}
std::string artifact(std::string_view parser, std::string_view source_set,
                     std::uint64_t first, std::uint64_t count,
                     std::string_view site, std::uint64_t ordinal) {
  std::string output{"OFF-REVIEWED-RETAIL-LOOKUP\0\1", 28};
  append_blob(output, parser);
  append_blob(output, source_set);
  append_u64(output, first);
  append_u64(output, count);
  append_u64(output, 1U);
  append_blob(output, site);
  append_u64(output, ordinal);
  return output;
}
void write(const std::filesystem::path &path, std::string_view contents) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  check(static_cast<bool>(output), "fixture writes");
}
} // namespace

int main() {
  using namespace off::ui::l10n;
  try {
    const std::filesystem::path root{OFF_TEST_WORK_DIR};
    const auto directory = root / "reviewed-retail-lookup";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(directory, error);
    check(!error, "fixture directory exists");
    const TranslationSourceBinding binding{"parser.fixture.v1", "source.fixture.v1",
                                           0U, 3U};
    const auto filename = directory / "reviewed-retail-lookup.offlookup";
    write(filename, artifact(binding.parser_identity, binding.source_set, 0U,
                             3U, "site.fixture.status", 2U));
    const auto accepted =
        load_local_reviewed_retail_lookup_artifacts(directory, binding);
    check(accepted.size() == 1U, "exact text-free source binding is admitted");
    check(RetailLookupSiteBindings::admit(binding, accepted).has_value(),
          "loaded artifact can bind only its reviewed opaque site");

    auto other_binding = binding;
    other_binding.ordinal_count = 4U;
    check(load_local_reviewed_retail_lookup_artifacts(directory, other_binding).empty(),
          "artifact cannot cross an ordinal-span boundary");

    auto trailing = artifact(binding.parser_identity, binding.source_set, 0U, 3U,
                             "site.fixture.status", 2U);
    trailing.push_back('\0');
    write(filename, trailing);
    check(load_local_reviewed_retail_lookup_artifacts(directory, binding).empty(),
          "trailing bytes are rejected");

    write(filename, artifact(binding.parser_identity, binding.source_set, 0U, 3U,
                             "site fixture invalid", 2U));
    check(load_local_reviewed_retail_lookup_artifacts(directory, binding).empty(),
          "non-opaque site labels are rejected");

    std::filesystem::remove_all(root, error);
    std::cout << "reviewed retail lookup artifact tests passed\n";
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
