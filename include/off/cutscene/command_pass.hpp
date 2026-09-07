#pragma once

#include "off/data/gms_image.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace off::cutscene {

// A conditional, already-admitted command phase, not a cut-sequence player.
// Registration order is explicit; callbacks must not destroy this owner.
class CommandPass final {
public:
    using Visitor = std::function<void(const data::GmsIntroCutCommandSource&, std::size_t)>;
    CommandPass(std::span<const data::GmsIntroCutCommandSource> registration_order, float derived_end);
    CommandPass(const CommandPass&) = delete;
    CommandPass& operator=(const CommandPass&) = delete;
    CommandPass(CommandPass&&) = delete;
    CommandPass& operator=(CommandPass&&) = delete;

    // Rejects reentrancy and positions at/above the empty-list sentinel as
    // explicit native safety policies. Callback exceptions retain the current
    // command for retry; earlier callbacks are not rolled back.
    void run(float position, const Visitor& visitor);
    void reset_start();

private:
    std::vector<data::GmsIntroCutCommandSource> commands_;
    std::vector<std::size_t> order_;
    std::optional<std::size_t> cursor_;
    std::int32_t next_position_{0};
    std::int32_t empty_sentinel_{0};
    std::int32_t exhausted_sentinel_{0};
    bool running_{false};
};

// Delivers one command that CommandPass has already selected.  This is kept
// separate from CommandPass so scheduling does not imply scene mutation.
enum class CommandDeliveryResult {
    delivered,
    event_unresolved,
    target_unresolved,
    no_target,
};

struct CommandDeliveryServices {
    // Resolves an authored event-table reference to the list owner's
    // registered 16-bit event identity.
    std::function<std::optional<std::uint16_t>(std::uint32_t)> resolve_event;
    // A nonzero authored reference selects this resolver exclusively.
    std::function<std::optional<std::uint64_t>(std::uint32_t)> resolve_reference;
    // This resolver is considered only when the authored reference is zero.
    std::function<std::optional<std::uint64_t>(std::string_view)> resolve_name;
    // The list owner performs the direct synchronous dispatch; this adapter
    // deliberately does not enqueue a generic scene event.
    std::function<void(std::uint64_t target, std::uint16_t event,
                       std::uint32_t argument, std::uint64_t sender)> direct_dispatch;
};

class CommandDeliveryAdapter final {
public:
    CommandDeliveryAdapter(CommandDeliveryServices services, std::uint64_t owner_sender);
    CommandDeliveryAdapter(const CommandDeliveryAdapter&) = delete;
    CommandDeliveryAdapter& operator=(const CommandDeliveryAdapter&) = delete;
    CommandDeliveryAdapter(CommandDeliveryAdapter&&) = delete;
    CommandDeliveryAdapter& operator=(CommandDeliveryAdapter&&) = delete;

    // Resolves the event first, then exactly one target route.  A target
    // resolution failure is not eligible for fallback.  Exceptions from the
    // direct dispatcher intentionally propagate synchronously.
    [[nodiscard]] CommandDeliveryResult deliver(
        const data::GmsIntroCutCommandSource& command) const;

private:
    CommandDeliveryServices services_;
    std::uint64_t owner_sender_{};
};

} // namespace off::cutscene
