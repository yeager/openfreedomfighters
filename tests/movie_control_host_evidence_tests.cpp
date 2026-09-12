#include "off/graphics/movie_control_host_evidence.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
template <typename Fn> bool rejects(Fn&& fn) {
  try { fn(); } catch (const std::runtime_error&) { return true; }
  return false;
}
}

int main() {
  using namespace off::graphics;
  MovieControlHostReaderReceipt owner{.owner_handle=91, .resource_handle=12,
      .source_directory_index=465, .source_offset=44, .component_index=8};
  auto component=owner;
  component.class_ordinal=3; component.requested_mask=0x20; component.priority=7;
  component.events={1,2,3,4,5,6,7};
  const auto evidence=MovieControlHostEvidence::from_reader_receipts(owner, component, 17);
  check(evidence.movie_component_handle()==17 && evidence.movie_owner_handle()==91 &&
      evidence.source_resource_handle()==12 && evidence.source_directory_index()==465,
      "matching receipts retain only live and source identities");
  check(evidence.movie_delay()==2048 &&
      MovieControlHostEvidence::scene_clock_units_per_second==1024,
      "host evidence supplies the fixed engine-clock protocol delay");
  check(rejects([&] { static_cast<void>(MovieControlHostEvidence::from_reader_receipts(
      owner, component, 0)); }), "a missing live component cannot create evidence");
  component.source_offset=45;
  check(rejects([&] { static_cast<void>(MovieControlHostEvidence::from_reader_receipts(
      owner, component, 17)); }), "mismatched reader provenance is rejected");
  component=owner; component.owner_handle=92;
  check(rejects([&] { static_cast<void>(MovieControlHostEvidence::from_reader_receipts(
      owner, component, 17)); }), "mismatched reader owner is rejected");
  owner.source_offset=0;
  check(rejects([&] { static_cast<void>(MovieControlHostEvidence::from_reader_receipts(
      owner, owner, 17)); }), "an unbound source offset is rejected");
  std::cout << "movie control host evidence tests passed\n";
}
