cmake_minimum_required(VERSION 3.25)

project(OpenFreedomFighters VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
include(FetchContent)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Vorbis REQUIRED)
find_package(ZLIB REQUIRED)

find_package(SDL3 3.2 CONFIG QUIET)
if(NOT SDL3_FOUND)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        SDL3
        URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.10/SDL3-3.4.10.tar.gz
        URL_HASH SHA256=12b34280415ec8418c864408b93d008a20a6530687ee613d60bfbd20411f2785
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

if(TARGET SDL3::SDL3-static)
    set(OFF_SDL3_TARGET SDL3::SDL3-static)
else()
    set(OFF_SDL3_TARGET SDL3::SDL3)
endif()

add_library(off_core
    src/audio/decode.cpp
    src/crypto/sha256.cpp
    src/data/archive_vfs.cpp
    src/data/audio_bank_header.cpp
    src/data/gms_image.cpp
    src/data/install.cpp
    src/data/packed_resource.cpp
    src/data/primitive_catalog.cpp
    src/data/render_map.cpp
    src/data/scene_support.cpp
    src/data/texture_catalog.cpp
    src/data/vfs_file_view.cpp
    src/data/zgf_bundle.cpp
    src/data/zip_archive.cpp
    src/graphics/texture_decode.cpp
    src/graphics/render_assets.cpp
    src/graphics/render_preview.cpp
)
target_include_directories(off_core PUBLIC include)
target_compile_features(off_core PUBLIC cxx_std_23)
target_link_libraries(off_core PUBLIC Vorbis::vorbisfile ZLIB::ZLIB)
set_target_properties(off_core PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
    target_compile_options(off_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(off_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

add_executable(
    openfreedomfighters
    src/main.cpp
    src/platform/sdl_gpu_runtime.cpp
)
target_link_libraries(openfreedomfighters PRIVATE off_core ${OFF_SDL3_TARGET})

install(TARGETS openfreedomfighters RUNTIME DESTINATION bin)

if(BUILD_TESTING)
    add_executable(off_core_tests tests/core_tests.cpp)
    target_link_libraries(off_core_tests PRIVATE off_core)
    target_compile_definitions(
        off_core_tests PRIVATE OFF_TEST_WORK_DIR="${CMAKE_BINARY_DIR}/test-work"
    )
    add_test(NAME off_core_tests COMMAND off_core_tests)

    add_executable(off_audio_bank_header_tests tests/audio_bank_header_tests.cpp)
    target_link_libraries(off_audio_bank_header_tests PRIVATE off_core)
    add_test(NAME off_audio_bank_header_tests COMMAND off_audio_bank_header_tests)

    add_executable(off_audio_decode_tests tests/audio_decode_tests.cpp)
    target_link_libraries(off_audio_decode_tests PRIVATE off_core Vorbis::vorbisenc)
    add_test(NAME off_audio_decode_tests COMMAND off_audio_decode_tests)

    add_executable(off_scene_support_tests tests/scene_support_tests.cpp)
    target_link_libraries(off_scene_support_tests PRIVATE off_core)
    add_test(NAME off_scene_support_tests COMMAND off_scene_support_tests)

    add_executable(off_texture_catalog_tests tests/texture_catalog_tests.cpp)
    target_link_libraries(off_texture_catalog_tests PRIVATE off_core)
    add_test(NAME off_texture_catalog_tests COMMAND off_texture_catalog_tests)

    add_executable(off_texture_decode_tests tests/texture_decode_tests.cpp)
    target_link_libraries(off_texture_decode_tests PRIVATE off_core)
    add_test(NAME off_texture_decode_tests COMMAND off_texture_decode_tests)

    add_executable(off_primitive_catalog_tests tests/primitive_catalog_tests.cpp)
    target_link_libraries(off_primitive_catalog_tests PRIVATE off_core)
    add_test(NAME off_primitive_catalog_tests COMMAND off_primitive_catalog_tests)

    add_executable(off_packed_resource_tests tests/packed_resource_tests.cpp)
    target_link_libraries(off_packed_resource_tests PRIVATE off_core)
    add_test(NAME off_packed_resource_tests COMMAND off_packed_resource_tests)

    add_executable(off_gms_image_tests tests/gms_image_tests.cpp)
    target_link_libraries(off_gms_image_tests PRIVATE off_core)
    add_test(NAME off_gms_image_tests COMMAND off_gms_image_tests)

    add_executable(off_render_map_tests tests/render_map_tests.cpp)
    target_link_libraries(off_render_map_tests PRIVATE off_core)
    add_test(NAME off_render_map_tests COMMAND off_render_map_tests)

    add_executable(off_render_assets_tests tests/render_assets_tests.cpp)
    target_link_libraries(off_render_assets_tests PRIVATE off_core)
    add_test(NAME off_render_assets_tests COMMAND off_render_assets_tests)

    add_executable(off_render_preview_tests tests/render_preview_tests.cpp)
    target_link_libraries(off_render_preview_tests PRIVATE off_core)
    add_test(NAME off_render_preview_tests COMMAND off_render_preview_tests)

    add_executable(off_zgf_bundle_tests tests/zgf_bundle_tests.cpp)
    target_link_libraries(off_zgf_bundle_tests PRIVATE off_core)
    add_test(NAME off_zgf_bundle_tests COMMAND off_zgf_bundle_tests)
endif()
