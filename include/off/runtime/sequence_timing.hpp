#pragma once

namespace off::runtime {
// A copy made while the attachment-owned timing record is live. The two
// values and option belong to different list-update passes.
struct SequenceTiming final {
  float first{}, second{};
  bool option{};
  bool operator==(const SequenceTiming&) const = default;
};
}
