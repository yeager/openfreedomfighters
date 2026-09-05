#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace off::data {

class SceneSupport final {
public:
    [[nodiscard]] static SceneSupport parse(std::span<const std::byte> bytes);

    [[nodiscard]] std::span<const std::string> dependencies() const noexcept {
        return dependencies_;
    }

private:
    std::vector<std::string> dependencies_;
};

}  // namespace off::data
