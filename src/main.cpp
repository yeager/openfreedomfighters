#include "off/data/install.hpp"
#include "off/mode.hpp"

#include <filesystem>
#include <iostream>
#include <string_view>

namespace {

void usage(std::ostream& output) {
    output << "Usage: openfreedomfighters --data PATH [--mode original|modern] [--verify-only]\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::filesystem::path data_path;
    auto mode = off::Mode::original;
    bool verify_only = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--data" && index + 1 < argc) {
            data_path = argv[++index];
        } else if (argument == "--mode" && index + 1 < argc) {
            const auto parsed = off::parse_mode(argv[++index]);
            if (!parsed) {
                std::cerr << "Unknown mode. Expected original or modern.\n";
                return 2;
            }
            mode = *parsed;
        } else if (argument == "--verify-only") {
            verify_only = true;
        } else if (argument == "--help" || argument == "-h") {
            usage(std::cout);
            return 0;
        } else if (argument == "--version") {
            std::cout << "OpenFreedomFighters 0.1.0\n";
            return 0;
        } else {
            std::cerr << "Unknown or incomplete argument: " << argument << '\n';
            usage(std::cerr);
            return 2;
        }
    }
    if (data_path.empty()) {
        std::cerr << "A legally purchased Freedom Fighters installation is required.\n";
        usage(std::cerr);
        return 2;
    }

    const auto verification = off::data::verify_install(data_path);
    if (!verification) {
        std::cerr << "Game-data verification failed: " << verification.message << '\n';
        return 3;
    }
    std::cout << verification.message << '\n'
              << "Mode: " << off::mode_name(mode) << '\n';
    if (verify_only) {
        return 0;
    }
    std::cerr << "The native runtime is not implemented yet. Use --verify-only during Phase 0.\n";
    return 4;
}

