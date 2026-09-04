#include "off/crypto/sha256.hpp"
#include "off/data/install.hpp"
#include "off/mode.hpp"

#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

}  // namespace

int main() {
    check(
        off::crypto::to_hex(off::crypto::sha256("")) ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "SHA-256 empty vector"
    );
    check(
        off::crypto::to_hex(off::crypto::sha256("abc")) ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "SHA-256 abc vector"
    );
    check(off::parse_mode("original") == off::Mode::original, "parse Original mode");
    check(off::parse_mode("modern") == off::Mode::modern, "parse Modern mode");
    check(!off::parse_mode("other"), "reject unknown mode");
    const auto missing = off::data::verify_install("path-that-must-not-exist");
    check(
        missing.error == off::data::InstallError::missing_root,
        "reject missing game-data directory"
    );
    return failures == 0 ? 0 : 1;
}

