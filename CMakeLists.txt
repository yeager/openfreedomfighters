cmake_minimum_required(VERSION 3.25)

project(OpenFreedomFighters VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
find_package(ZLIB REQUIRED)

add_library(off_core
    src/crypto/sha256.cpp
    src/data/archive_vfs.cpp
    src/data/install.cpp
    src/data/zip_archive.cpp
)
target_include_directories(off_core PUBLIC include)
target_compile_features(off_core PUBLIC cxx_std_23)
target_link_libraries(off_core PUBLIC ZLIB::ZLIB)
set_target_properties(off_core PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
    target_compile_options(off_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(off_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

add_executable(openfreedomfighters src/main.cpp)
target_link_libraries(openfreedomfighters PRIVATE off_core)

install(TARGETS openfreedomfighters RUNTIME DESTINATION bin)

if(BUILD_TESTING)
    add_executable(off_core_tests tests/core_tests.cpp)
    target_link_libraries(off_core_tests PRIVATE off_core)
    target_compile_definitions(
        off_core_tests PRIVATE OFF_TEST_WORK_DIR="${CMAKE_BINARY_DIR}/test-work"
    )
    add_test(NAME off_core_tests COMMAND off_core_tests)
endif()
