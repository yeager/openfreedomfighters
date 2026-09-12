#include "off/data/loc_catalog.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

// This target receives only the libFuzzer-provided byte span.  It deliberately
// has no seed corpus, file access, logging, or retail-installation discovery.
// The input cap keeps a single mutation from requesting an impractical parser
// allocation while still exercising the production parser's complete framing
// and rejection paths.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size) {
  constexpr std::size_t maximum_fuzz_input_bytes = 1024U * 1024U;
  if (data == nullptr || size > maximum_fuzz_input_bytes)
    return 0;

  const std::span input{data, size};
  static_cast<void>(off::data::decode_loc_member_display_texts(
      std::as_bytes(input)));
  return 0;
}
