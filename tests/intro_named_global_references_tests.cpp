#include "off/graphics/intro_named_global_references.hpp"
#include "off/graphics/intro_outer_loader_tail_readiness.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <map>

namespace {
using namespace off::graphics;
using Block = std::array<std::byte, 16>;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

template<class Operation> void rejects(Operation operation) {
  bool rejected{};
  try { operation(); } catch (const std::runtime_error&) { rejected = true; }
  check(rejected, "unsupported named-reference input must throw");
}

Block null_block() {
  Block result{};
  result[0] = std::byte{16};
  result[4] = result[9] = std::byte{0x08};
  result[14] = std::byte{0x06};
  result[15] = std::byte{0xff};
  return result;
}
}

int main() {
  auto block = null_block();
  std::string label = "FixtureCamera,FixtureListener";
  const auto entries = read_intro_named_global_null_references({label, block});
  label.assign("Replaced");
  block.fill(std::byte{0xff});
  check(entries.names == std::array<std::string, 2>{"FixtureCamera", "FixtureListener"} &&
            entries.registry_type == 16U && entries.reference_bits == 0U,
        "typed null entries own both source names and have reference type 16");

  block = null_block();
  const auto spaces = read_intro_named_global_null_references({" One,Two ", block});
  check(spaces.names == std::array<std::string, 2>{" One", "Two "},
        "source name splitting does not trim whitespace");
  for (const auto bad_label : {"", "One", ",Two", "One,", "One,Two,Three", "One,oNE"})
    rejects([&] { (void)read_intro_named_global_null_references({bad_label, block}); });
  for (const auto byte : {0U, 0x1fU, 0x7fU, 0x80U, 0xffU}) {
    std::string bad = "One,Two";
    bad[1] = static_cast<char>(byte);
    rejects([&] { (void)read_intro_named_global_null_references({bad, block}); });
  }
  const auto oversized = std::string(1025, 'A') + ",B";
  rejects([&] { (void)read_intro_named_global_null_references({oversized, block}); });
  for (std::size_t size = 0; size < block.size(); ++size)
    rejects([&] { (void)read_intro_named_global_null_references(
        {"One,Two", std::span<const std::byte>(block).first(size)}); });
  for (const auto offset : {0U, 1U, 2U, 3U, 5U, 10U, 14U, 15U}) {
    auto bad = block;
    bad[offset] ^= std::byte{1};
    rejects([&] { (void)read_intro_named_global_null_references({"One,Two", bad}); });
  }
  for (const auto offset : {4U, 9U}) {
    for (const auto tag : {0x03U, 0x0aU, 0x0bU, 0x48U, 0x88U, 0xc8U}) {
      auto bad = block;
      bad[offset] = static_cast<std::byte>(tag);
      rejects([&] { (void)read_intro_named_global_null_references({"One,Two", bad}); });
    }
  }
  // Section padding is outside the complete tagged block and must not become
  // a third value. This fixture is independently authored.
  std::vector<std::byte> section;
  for (const char byte : std::string("One,Two")) section.push_back(static_cast<std::byte>(byte));
  section.push_back(std::byte{});
  section.insert(section.end(), block.begin(), block.end());
  section.insert(section.end(), 3U, std::byte{0x55});
  check(read_intro_named_global_null_references(parse_intro_named_global_section_envelope(section)).names ==
            std::array<std::string, 2>{"One", "Two"},
        "section padding is excluded by the bounded envelope");
  off::data::GmsOuterLoaderSources sources;
  sources.named_global = section;
  const auto readiness = inspect_intro_outer_loader_tail_readiness(sources);
  check(readiness.named_global_native_supported && !readiness.ready_to_run() &&
            readiness.required_boundaries.size() == 5U,
        "native named reader support does not imply other loader-tail services exist");
  sources.named_global->at(8U + 10U) = std::byte{1};
  const auto unsupported = inspect_intro_outer_loader_tail_readiness(sources);
  check(!unsupported.named_global_native_supported && !unsupported.ready_to_run() &&
            unsupported.required_boundaries.front() ==
                IntroOuterLoaderTailBoundary::named_global_relocation_and_reader,
        "unapproved nonzero references retain an explicit required reader boundary");

  std::map<std::string, unsigned, IntroPropertyNameLess> names;
  names.emplace("FixtureCamera", 1U);
  names.insert_or_assign("fIXTUREcAMERA", 2U);
  names.emplace("FixtureCameraLong", 3U);
  names.emplace("Fixture", 4U);
  check(names.size() == 3U && names.find(std::string_view("FIXTURECAMERA"))->second == 2U &&
            names.find("FixtureCamer") == names.end(),
        "ASCII property matching folds case and compares the full name");
  std::cout << "intro named-global reference tests passed\n";
}
