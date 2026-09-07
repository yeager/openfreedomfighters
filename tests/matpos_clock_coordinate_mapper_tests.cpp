#include "off/graphics/matpos_clock_coordinate_mapper.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>

namespace {
using namespace off::graphics;
int failures{};
void check(bool value, const char* message) {
    if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
void coordinate(const std::optional<float>& value, float expected,
                const char* message) {
    check(value && *value == expected, message);
}
}

int main() {
    const MatPosClockRange forward{10.0F, 20.0F, 3072.9F, false};
    const MatPosKeysCoordinateDescriptor keys{4, 40, 2};
    coordinate(map_matpos_clock_to_coordinate(100, 100, forward, keys), 3.0F,
               "origin uses the retained endpoint, not KEYS lower frame");
    coordinate(map_matpos_clock_to_coordinate(100, 101, forward, keys), 4.5F,
               "forward frame uses elapsed times truncated rate");
    coordinate(map_matpos_clock_to_coordinate(100, 99, forward, keys), 3.0F,
               "earlier clock freezes elapsed at zero");

    coordinate(map_matpos_clock_to_coordinate(100, 104, forward, keys), 9.0F,
               "endpoint overrun remains coordinate-only until terminal predicate is recovered");

    const MatPosClockRange reverse{20.0F, 10.0F, 3072.9F, false};
    coordinate(map_matpos_clock_to_coordinate(100, 100, reverse, keys), 8.0F,
               "reverse origin still uses retained endpoint rather than KEYS lower frame");
    coordinate(map_matpos_clock_to_coordinate(100, 101, reverse, keys), 6.5F,
               "reverse arithmetic retains no lower coordinate clamp");

    const MatPosClockRange forward_loop{10.0F, 14.0F, 3072.0F, true};
    coordinate(map_matpos_clock_to_coordinate(0, 1, forward_loop, keys), 3.5F,
               "forward loop uses fixed-point signed remainder from lower base");
    const MatPosClockRange reverse_loop{14.0F, 10.0F, 3072.0F, true};
    coordinate(map_matpos_clock_to_coordinate(0, 1, reverse_loop, keys), 4.5F,
               "reverse loop selects upper base and signed remainder");

    const MatPosClockRange clamped{0.0F, 100.0F, 1024.0F, false};
    const MatPosKeysCoordinateDescriptor short_keys{0, 3, 2};
    coordinate(map_matpos_clock_to_coordinate(0, 5, clamped, short_keys), 2.0F,
               "coordinate clamps only its upper edge");

    auto invalid = forward;
    invalid.rate = std::numeric_limits<float>::infinity();
    check(!map_matpos_clock_to_coordinate(0, 0, invalid, keys), "non-finite rate fails closed");
    auto invalid_keys = keys; invalid_keys.count = 0;
    check(!map_matpos_clock_to_coordinate(0, 0, forward, invalid_keys), "zero count fails closed");
    invalid_keys = keys; invalid_keys.divisor = 0;
    check(!map_matpos_clock_to_coordinate(0, 0, forward, invalid_keys), "zero divisor fails closed");
    invalid = forward; invalid.control_loop = true; invalid.upper_endpoint = invalid.lower_endpoint;
    check(!map_matpos_clock_to_coordinate(0, 0, invalid, keys), "zero loop modulus fails closed");

    if (failures != 0) return 1;
    std::cout << "Detached MatPos clock-coordinate mapper arithmetic verified.\n";
}
