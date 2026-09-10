#include "off/data/animation_image.hpp"

#include <cstddef>
#include <cstdint>
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
  std::vector<std::byte> bytes(24U, std::byte{0});
  set_u32(bytes, 0, 0x00414e4dU);
  set_u32(bytes, 8, static_cast<std::uint32_t>(bytes.size()));
  set_u32(bytes, 12, 12U);
  set_u32(bytes, 16, 10U);
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

int main() {
  const auto bytes = fixture();
  const auto image = off::data::AnimationImage::parse(bytes);
  check(image.header().byte_size == bytes.size(), "retain animation byte size");
  check(image.header().major_version == 12U && image.header().minor_version == 10U,
        "retain supported animation version");
  check_rejected([](auto &value) { set_u32(value, 0, 0); },
                 "reject wrong animation signature");
  check_rejected([](auto &value) { set_u32(value, 8, 20U); },
                 "reject declared-size mismatch");
  check_rejected([](auto &value) { set_u32(value, 12, 11U); },
                 "reject unsupported animation major version");
  check_rejected([](auto &value) { set_u32(value, 16, 11U); },
                 "reject unsupported animation minor version");
  auto truncated = fixture();
  truncated.resize(19U);
  try {
    static_cast<void>(off::data::AnimationImage::parse(truncated));
    check(false, "reject truncated animation header");
  } catch (const std::runtime_error &) {
  }
  return failures == 0 ? 0 : 1;
}
