#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace off::graphics {

// Stable live identities collected after intro directory construction. These
// are coverage keys only: they cannot carry an implementation callback.
struct IntroReaderAdmissionIdentity {
  std::uint64_t resource{};
  std::uint32_t source_offset{};
  std::size_t source_directory_index{};
  bool operator==(const IntroReaderAdmissionIdentity&) const = default;
};

struct IntroComponentAdmissionIdentity {
  std::uint64_t component{};
  bool operator==(const IntroComponentAdmissionIdentity&) const = default;
};

struct IntroOwnerAdmissionIdentity {
  std::uint64_t owner{};
  bool operator==(const IntroOwnerAdmissionIdentity&) const = default;
};

struct IntroLifecycleAdmissionRequirements {
  std::vector<IntroReaderAdmissionIdentity> readers;
  std::vector<IntroComponentAdmissionIdentity> components;
  std::vector<IntroOwnerAdmissionIdentity> owners;
};

enum class IntroLifecycleAdmissionFailure : std::uint8_t {
  none,
  reader_coverage,
  component_coverage,
  owner_coverage,
};

struct IntroLifecycleAdmissionReport {
  std::size_t expected_readers{}, covered_readers{};
  std::size_t expected_components{}, covered_components{};
  std::size_t expected_owners{}, covered_owners{};
  IntroLifecycleAdmissionFailure failure{IntroLifecycleAdmissionFailure::reader_coverage};
  [[nodiscard]] bool ready() const noexcept {
    return failure == IntroLifecycleAdmissionFailure::none;
  }
};

// Fail-closed registry for real typed lifecycle implementations. Registering a
// key records coverage, but does not itself execute a reader or factory. The
// caller must install and invoke the corresponding typed implementation.
class IntroLifecycleAdmissionCoverageRegistry final {
public:
  explicit IntroLifecycleAdmissionCoverageRegistry(
      IntroLifecycleAdmissionRequirements requirements);

  void cover_reader(IntroReaderAdmissionIdentity identity);
  void cover_component(IntroComponentAdmissionIdentity identity);
  void cover_owner(IntroOwnerAdmissionIdentity identity);

  [[nodiscard]] IntroLifecycleAdmissionReport report() const noexcept;

private:
  IntroLifecycleAdmissionRequirements requirements_;
  std::vector<IntroReaderAdmissionIdentity> covered_readers_;
  std::vector<IntroComponentAdmissionIdentity> covered_components_;
  std::vector<IntroOwnerAdmissionIdentity> covered_owners_;
};

} // namespace off::graphics
