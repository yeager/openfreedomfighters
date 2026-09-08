#include "off/graphics/intro_lifecycle_admission.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>

namespace {

using namespace off::graphics;

void check(bool value, const char* message) {
  if (!value)
    throw std::runtime_error(message);
}

template <class Callback>
void rejects(Callback callback) {
  bool rejected = false;
  try {
    callback();
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  check(rejected, "operation must reject");
}

IntroLifecycleAdmissionRequirements requirements() {
  return {{{11, 17, 3}, {12, 19, 4}}, {{31}, {32}}, {{41}, {42}}};
}

} // namespace

int main() {
  try {
    rejects([] {
      auto invalid = requirements();
      invalid.readers.push_back(invalid.readers.front());
      static_cast<void>(IntroLifecycleAdmissionCoverageRegistry{std::move(invalid)});
    });

    IntroLifecycleAdmissionCoverageRegistry registry{requirements()};
    const auto initial = registry.report();
    check(initial.expected_readers == 2 && initial.covered_readers == 0 &&
              initial.expected_components == 2 && initial.covered_components == 0 &&
              initial.expected_owners == 2 && initial.covered_owners == 0 &&
              initial.failure == IntroLifecycleAdmissionFailure::reader_coverage,
          "fresh requirements fail reader coverage first");

    rejects([&] { registry.cover_reader({99, 1, 0}); });
    check(registry.report().covered_readers == 0,
          "unknown registration preserves coverage");
    registry.cover_reader({12, 19, 4});
    rejects([&] { registry.cover_reader({12, 19, 4}); });
    registry.cover_reader({11, 17, 3});
    check(registry.report().failure == IntroLifecycleAdmissionFailure::component_coverage,
          "complete reader coverage exposes component gap");

    registry.cover_component({32});
    registry.cover_component({31});
    check(registry.report().failure == IntroLifecycleAdmissionFailure::owner_coverage,
          "complete component coverage exposes owner gap");
    registry.cover_owner({42});
    registry.cover_owner({41});
    const auto complete = registry.report();
    check(complete.ready() && complete.expected_readers == complete.covered_readers &&
              complete.expected_components == complete.covered_components &&
              complete.expected_owners == complete.covered_owners,
          "only exact coverage becomes ready");
    rejects([&] { registry.cover_owner({41}); });
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
