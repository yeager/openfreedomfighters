#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace off::data {

// The ANM payload is retained as an opaque animation resource for now. This
// header is the complete, independently verified envelope shared by the
// supported installation; tracks and bindings remain unrecovered.
struct AnimationImageHeader {
  std::size_t byte_size{};
  std::uint32_t reference_table_count{};
  std::uint32_t format_value{};
};

struct AnimationReferenceTable {
  std::string name;
  std::vector<std::uint32_t> reference_words;
};

struct AnimationDescriptor {
  std::uint32_t opaque_word_0{};
  std::uint32_t opaque_word_1{};
  std::uint32_t tag{};
};

struct AnimationByteRange {
  std::size_t offset{};
  std::size_t byte_size{};
};

struct OpaqueAnimationSection {
  std::uint32_t preceding_tag{};
  AnimationByteRange payload;
  std::vector<AnimationByteRange> components;
};

class AnimationImage final {
public:
  [[nodiscard]] static AnimationImage parse(std::span<const std::byte> bytes);

  [[nodiscard]] const AnimationImageHeader &header() const noexcept {
    return header_;
  }
  [[nodiscard]] std::span<const AnimationReferenceTable>
  reference_tables() const noexcept {
    return reference_tables_;
  }
  [[nodiscard]] std::span<const AnimationDescriptor>
  descriptors() const noexcept {
    return descriptors_;
  }
  [[nodiscard]] std::span<const OpaqueAnimationSection>
  sections() const noexcept {
    return sections_;
  }

private:
  AnimationImageHeader header_{};
  std::vector<AnimationReferenceTable> reference_tables_;
  std::vector<AnimationDescriptor> descriptors_;
  std::vector<OpaqueAnimationSection> sections_;
};

} // namespace off::data
