#include "off/cutscene/picture_activation_prefix.hpp"

#include <stdexcept>

namespace off::cutscene {

void PictureActivationPrefix::run(std::uint32_t& target_flags,
                                  std::optional<std::uint32_t> parent_flags,
                                  bool owner_present,
                                  const Visitor& visitor) {
    const bool hidden = (target_flags & 0x400U) != 0U;
    // Missing-parent validation before effects is a native safety policy.
    if (running_ || !visitor || (hidden && !parent_flags))
        throw std::runtime_error("picture activation prefix input or reentrancy is unsupported");
    struct Guard {
        bool& flag;
        explicit Guard(bool& value) : flag(value) { flag = true; }
        ~Guard() { flag = false; }
    } guard(running_);
    visitor(Stage::append_tracking);
    if (!hidden) {
        visitor(Stage::record_not_requested);
        return;
    }
    if ((*parent_flags & 0x400U) != 0U) {
        visitor(Stage::parent_blocked);
    } else {
        target_flags &= ~0x400U;
        if ((target_flags & 0x200c00U) == 0U && (target_flags & 0x40000U) != 0U)
            visitor(Stage::class_registration);
        if (owner_present) visitor(Stage::owner_activation);
        if ((target_flags & 0x200c00U) == 0U) visitor(Stage::normal_registration);
    }
    visitor(Stage::phase_one);
    visitor(Stage::record_requested);
}

} // namespace off::cutscene
