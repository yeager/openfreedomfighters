#include "off/graphics/intro_lifecycle_admission.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace off::graphics {
namespace {

template <class Identity>
void require_valid_unique(const std::vector<Identity>& identities,
                          const char* description) {
  for (std::size_t index = 0; index < identities.size(); ++index)
    if (std::count(identities.begin(), identities.end(), identities[index]) != 1)
      throw std::runtime_error(std::string("intro lifecycle requirements contain duplicate ") + description);
}

void require_live_reader(const IntroReaderAdmissionIdentity& identity) {
  // A source-directory index may legitimately be zero. The other two fields
  // are the recovered runtime resource and deferred-block boundary, however:
  // permitting either sentinel would let a fabricated requirement become
  // "covered" without a source-backed reader receipt.
  if (identity.resource == 0 || identity.source_offset == 0)
    throw std::runtime_error(
        "intro lifecycle requirements contain an unbound reader identity");
}

void require_live_component(const IntroComponentAdmissionIdentity& identity) {
  if (identity.component == 0)
    throw std::runtime_error(
        "intro lifecycle requirements contain an unbound component identity");
}

void require_live_owner(const IntroOwnerAdmissionIdentity& identity) {
  if (identity.owner == 0)
    throw std::runtime_error(
        "intro lifecycle requirements contain an unbound owner identity");
}

template <class Identity>
void cover(const std::vector<Identity>& requirements, std::vector<Identity>& covered,
           Identity identity, const char* description) {
  if (std::find(requirements.begin(), requirements.end(), identity) == requirements.end())
    throw std::runtime_error(std::string("intro lifecycle registration is not required: ") + description);
  if (std::find(covered.begin(), covered.end(), identity) != covered.end())
    throw std::runtime_error(std::string("intro lifecycle registration is duplicated: ") + description);
  covered.push_back(identity);
}

} // namespace

IntroLifecycleAdmissionCoverageRegistry::IntroLifecycleAdmissionCoverageRegistry(
    IntroLifecycleAdmissionRequirements requirements)
    : requirements_(std::move(requirements)) {
  require_valid_unique(requirements_.readers, "reader");
  require_valid_unique(requirements_.components, "component");
  require_valid_unique(requirements_.owners, "owner");
  for (const auto& identity : requirements_.readers)
    require_live_reader(identity);
  for (const auto& identity : requirements_.components)
    require_live_component(identity);
  for (const auto& identity : requirements_.owners)
    require_live_owner(identity);
}

void IntroLifecycleAdmissionCoverageRegistry::cover_reader(
    IntroReaderAdmissionIdentity identity) {
  cover(requirements_.readers, covered_readers_, identity, "reader");
}

void IntroLifecycleAdmissionCoverageRegistry::cover_component(
    IntroComponentAdmissionIdentity identity) {
  cover(requirements_.components, covered_components_, identity, "component");
}

void IntroLifecycleAdmissionCoverageRegistry::cover_owner(
    IntroOwnerAdmissionIdentity identity) {
  cover(requirements_.owners, covered_owners_, identity, "owner");
}

IntroLifecycleAdmissionReport IntroLifecycleAdmissionCoverageRegistry::report() const noexcept {
  IntroLifecycleAdmissionReport result{
      requirements_.readers.size(), covered_readers_.size(),
      requirements_.components.size(), covered_components_.size(),
      requirements_.owners.size(), covered_owners_.size()};
  result.failure = result.covered_readers != result.expected_readers
                       ? IntroLifecycleAdmissionFailure::reader_coverage
                       : result.covered_components != result.expected_components
                             ? IntroLifecycleAdmissionFailure::component_coverage
                             : result.covered_owners != result.expected_owners
                                   ? IntroLifecycleAdmissionFailure::owner_coverage
                                   : IntroLifecycleAdmissionFailure::none;
  return result;
}

} // namespace off::graphics
