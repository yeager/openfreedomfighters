#include "off/data/oct_image.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

void put_u32(std::vector<std::byte> &bytes, std::size_t offset,
             std::uint32_t value) {
  for (unsigned shift{}; shift < 32U; shift += 8U)
    bytes.at(offset + shift / 8U) =
        static_cast<std::byte>((value >> shift) & 0xffU);
}

std::vector<std::byte> fixture(bool present) {
  std::vector<std::byte> bytes(100U);
  for (const auto offset : {0U, 4U, 8U, 12U})
    put_u32(bytes, offset, present ? 1U : 0U);
  if (present) {
    put_u32(bytes, 16U, 80U);
    put_u32(bytes, 48U, 52U);
  }
  return bytes;
}

template <typename Operation> void rejects(Operation operation) {
  try {
    operation();
  } catch (const std::runtime_error &) {
    return;
  }
  throw std::runtime_error("expected OCT envelope rejection");
}

void check(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

} // namespace

int main() {
  try {
    const auto present = off::data::OctImage::parse(fixture(true));
    check(present.envelope().byte_size == 100U &&
              present.envelope().repeated_header_flag &&
              present.envelope().structural_bounds &&
              present.envelope().structural_bounds->lower == 52U &&
              present.envelope().structural_bounds->upper == 80U,
          "retain only validated OCT envelope boundaries");
    const auto absent = off::data::OctImage::parse(fixture(false));
    check(!absent.envelope().repeated_header_flag &&
              !absent.envelope().structural_bounds,
          "zero OCT structural-boundary pair remains absent");

    rejects([] {
      static_cast<void>(off::data::OctImage::parse(std::vector<std::byte>(99U)));
    });
    rejects([] {
      static_cast<void>(off::data::OctImage::parse(std::vector<std::byte>(104U)));
    });
    for (const auto mutation :
         {std::pair{0U, 2U}, std::pair{4U, 0U}, std::pair{36U, 1U},
          std::pair{40U, 1U}, std::pair{44U, 1U}, std::pair{16U, 0U},
          std::pair{48U, 51U}, std::pair{48U, 53U}, std::pair{16U, 100U}}) {
      auto bytes = fixture(true);
      put_u32(bytes, mutation.first, mutation.second);
      rejects([&] { static_cast<void>(off::data::OctImage::parse(bytes)); });
    }
    auto reversed = fixture(true);
    put_u32(reversed, 16U, 52U);
    put_u32(reversed, 48U, 80U);
    rejects([&] { static_cast<void>(off::data::OctImage::parse(reversed)); });
    std::cout << "OCT envelope tests passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
