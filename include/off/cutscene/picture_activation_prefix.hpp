#pragma once

#include <cstdint>
#include <functional>
#include <optional>

namespace off::cutscene {

// Only the reviewed new-tracking, existing-object, no-replacement plain-picture
// resolver prefix. Caller proves identity/lifetime and excludes group recursion.
class PictureActivationPrefix final {
public:
    enum class Stage {
        append_tracking, parent_blocked, class_registration,
        owner_activation, normal_registration, phase_one,
        record_requested, record_not_requested
    };
    using Visitor = std::function<void(Stage)>;
    PictureActivationPrefix() = default;
    PictureActivationPrefix(const PictureActivationPrefix&) = delete;
    PictureActivationPrefix& operator=(const PictureActivationPrefix&) = delete;
    PictureActivationPrefix(PictureActivationPrefix&&) = delete;
    PictureActivationPrefix& operator=(PictureActivationPrefix&&) = delete;

    // Flags are runtime flags, NOT the unconverted authored source word.
    // class_registration represents the entire conditional registration-record
    // helper; no allocation or maintenance semantics are invented inside it.
    // Callbacks must not mutate inputs, destroy the target or reenter this object.
    // Exceptions retain completed tracking/flag effects, with no forced callbacks.
    void run(std::uint32_t& target_flags, std::optional<std::uint32_t> parent_flags,
             bool registration_class, bool owner_present, const Visitor& visitor);

private:
    bool running_{false};
};

} // namespace off::cutscene
