#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace off::graphics {
class IntroRuntime;

// A content-free receipt copied from the two independently checked controller
// readers. It carries identity/provenance only; it does not reinterpret an
// authored option or source payload as a timeout.
struct MovieControlHostReaderReceipt {
  std::uint64_t owner_handle{};
  std::uint64_t resource_handle{};
  std::size_t source_directory_index{};
  std::uint32_t source_offset{};
  std::size_t component_index{};
  std::uint16_t class_ordinal{};
  std::uint32_t requested_mask{};
  std::uint32_t priority{};
  std::array<std::uint16_t, 7> events{};
};

// The native timeout is a fixed protocol value produced by MovieControl phase
// two, rather than an authored controller field. The signed add itself remains
// in MovieControlFirstUpdate; this value only closes the source-receipt gate
// required before a later host can be constructed.
class MovieControlHostEvidence final {
 public:
  static constexpr std::int32_t compatibility_delay = 2048;
  static constexpr std::int32_t scene_clock_units_per_second = 1024;

  [[nodiscard]] static MovieControlHostEvidence from_reader_receipts(
      const MovieControlHostReaderReceipt& owner_reader,
      const MovieControlHostReaderReceipt& component_reader,
      std::uint64_t live_component_handle);
  // Requires the complete ordinary reader bracket and both actual controller
  // receipts in the supplied retained scene. It only returns evidence; it does
  // not create a host, enter lifecycle work, or call a controller service.
  [[nodiscard]] static MovieControlHostEvidence from_runtime(
      const IntroRuntime& runtime);

  [[nodiscard]] std::uint64_t movie_component_handle() const noexcept {
    return movie_component_handle_;
  }
  [[nodiscard]] std::uint64_t movie_owner_handle() const noexcept {
    return movie_owner_handle_;
  }
  [[nodiscard]] std::int32_t movie_delay() const noexcept {
    return compatibility_delay;
  }
  [[nodiscard]] std::uint64_t source_resource_handle() const noexcept {
    return source_resource_handle_;
  }
  [[nodiscard]] std::size_t source_directory_index() const noexcept {
    return source_directory_index_;
  }

 private:
  MovieControlHostEvidence(std::uint64_t component, std::uint64_t owner,
                           std::uint64_t resource, std::size_t directory)
      : movie_component_handle_(component), movie_owner_handle_(owner),
        source_resource_handle_(resource), source_directory_index_(directory) {}
  std::uint64_t movie_component_handle_{};
  std::uint64_t movie_owner_handle_{};
  std::uint64_t source_resource_handle_{};
  std::size_t source_directory_index_{};
};
}  // namespace off::graphics
