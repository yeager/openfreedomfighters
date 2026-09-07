#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>

namespace off::runtime {

// A provider owns both its child map and the returned child objects.  A
// component binding borrows this interface; it never owns, copies, or caches a
// child result.  The interface intentionally exposes only the recovered exact
// named lookup boundary, not a generic component/source lookup.
class OwnerComponentProvider {
public:
    virtual ~OwnerComponentProvider() = default;
    [[nodiscard]] virtual const void* find_child_exact(std::array<char, 4> key) const noexcept = 0;
};

class OwnerComponentProviderBindings;

// One factory-produced component's borrowed association with its live owner.
// A binding is single-use: it may be bound once and invalidated once, and is
// never rebound to a different provider.  The host must invalidate bindings
// before base component cleanup and before provider destruction.
class OwnerComponentProviderBinding final {
public:
    OwnerComponentProviderBinding() = default;
    ~OwnerComponentProviderBinding() { invalidate(); }
    OwnerComponentProviderBinding(const OwnerComponentProviderBinding&) = delete;
    OwnerComponentProviderBinding& operator=(const OwnerComponentProviderBinding&) = delete;

    [[nodiscard]] bool bound() const noexcept { return provider_ != nullptr; }
    [[nodiscard]] bool invalidated() const noexcept { return invalidated_; }

    void bind(OwnerComponentProvider& provider, OwnerComponentProviderBindings& bindings);
    void invalidate() noexcept;

    // Each request reaches the currently-bound provider.  In particular, a
    // prior result cannot remain usable after invalidation or provider teardown.
    [[nodiscard]] const void* find_required_keys_child() const noexcept {
        return provider_ == nullptr ? nullptr : provider_->find_child_exact(keys_name);
    }

private:
    friend class OwnerComponentProviderBindings;

    void invalidate_from_provider(OwnerComponentProviderBindings& bindings) noexcept;

    static constexpr std::array<char, 4> keys_name{'K', 'E', 'Y', 'S'};
    OwnerComponentProvider* provider_{};
    OwnerComponentProviderBindings* bindings_{};
    bool invalidated_{};
};

// Provider-side enrollment.  This is deliberately an explicit teardown
// operation: automatic cross-owner invalidation was not recovered, so a host
// must call invalidate_all() before it tears down the provider itself.
class OwnerComponentProviderBindings final {
public:
    OwnerComponentProviderBindings() = default;
    OwnerComponentProviderBindings(const OwnerComponentProviderBindings&) = delete;
    OwnerComponentProviderBindings& operator=(const OwnerComponentProviderBindings&) = delete;

    void invalidate_all() noexcept {
        const auto enrolled = bindings_;
        for (auto* binding : enrolled) {
            if (binding != nullptr) {
                binding->invalidate_from_provider(*this);
            }
        }
    }

    [[nodiscard]] std::size_t size() const noexcept { return bindings_.size(); }

private:
    friend class OwnerComponentProviderBinding;

    void enroll(OwnerComponentProviderBinding& binding) {
        if (std::find(bindings_.begin(), bindings_.end(), &binding) != bindings_.end()) {
            throw std::runtime_error("owner component provider binding is already enrolled");
        }
        bindings_.push_back(&binding);
    }

    void remove(OwnerComponentProviderBinding& binding) noexcept {
        const auto found = std::find(bindings_.begin(), bindings_.end(), &binding);
        if (found != bindings_.end()) {
            bindings_.erase(found);
        }
    }

    std::vector<OwnerComponentProviderBinding*> bindings_{};
};

inline void OwnerComponentProviderBinding::bind(
    OwnerComponentProvider& provider, OwnerComponentProviderBindings& bindings) {
    if (provider_ != nullptr || bindings_ != nullptr || invalidated_) {
        throw std::runtime_error("owner component provider binding cannot be rebound");
    }
    bindings.enroll(*this);
    provider_ = &provider;
    bindings_ = &bindings;
}

inline void OwnerComponentProviderBinding::invalidate() noexcept {
    if (bindings_ != nullptr) {
        bindings_->remove(*this);
    }
    provider_ = nullptr;
    bindings_ = nullptr;
    invalidated_ = true;
}

inline void OwnerComponentProviderBinding::invalidate_from_provider(
    OwnerComponentProviderBindings& bindings) noexcept {
    if (bindings_ == &bindings) {
        invalidate();
    }
}

} // namespace off::runtime
