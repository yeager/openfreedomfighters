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

# SDL_ttf provides both the portable in-memory retail-font rasterizer and one
# small, zlib-licensed precompiled shader pair in every SDL GPU format.
FetchContent_Declare(
    off_shader_assets
    URL https://github.com/libsdl-org/SDL_ttf/releases/download/release-3.2.2/SDL3_ttf-3.2.2.tar.gz
    URL_HASH SHA256=63547d58d0185c833213885b635a2c0548201cc8f301e6587c0be1a67e1e045d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR off-no-cmake-project
)
FetchContent_MakeAvailable(off_shader_assets)

set(SDLTTF_INSTALL OFF CACHE BOOL "" FORCE)
set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
set(SDLTTF_VENDORED OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(
    "${off_shader_assets_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/off-sdl3-ttf"
    EXCLUDE_FROM_ALL
)

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
    src/data/picture_resource.cpp
    src/data/picture_texture_binding.cpp
    src/data/startup_graphics_composition.cpp
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
    src/graphics/scene_render.cpp
    src/graphics/scene_gpu_plan.cpp
    src/graphics/startup_graphics_asset.cpp
    src/graphics/startup_graphics_prepared_plan.cpp
    src/graphics/startup_graphics_expanded_plan.cpp
    src/graphics/picture_transform.cpp
    src/graphics/picture_expansion.cpp
    src/platform/startup_lifecycle.cpp
    src/settings/graphics_settings.cpp
    src/simulation/fixed_step.cpp
    src/simulation/input.cpp
    src/simulation/world.cpp
    src/ui/graphics_menu.cpp
    src/ui/graphics_menu_draw.cpp
    src/ui/retail_ui_fonts.cpp
    src/ui/retail_ui_textures.cpp
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
    src/platform/sdl_startup.cpp
)
target_link_libraries(
    openfreedomfighters PRIVATE
    off_core ${OFF_SDL3_TARGET} SDL3_ttf::SDL3_ttf
)
target_include_directories(
    openfreedomfighters PRIVATE
    "${off_shader_assets_SOURCE_DIR}/examples"
)
add_custom_target(off_startup_assets
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:openfreedomfighters>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/branding/openfreedomfighters-splash.bmp"
            "$<TARGET_FILE_DIR:openfreedomfighters>/assets/openfreedomfighters-splash.bmp"
    VERBATIM
)
add_dependencies(openfreedomfighters off_startup_assets)
if(MSVC)
    target_compile_options(openfreedomfighters PRIVATE /W4 /permissive-)
else()
    target_compile_options(
        openfreedomfighters PRIVATE -Wall -Wextra -Wpedantic -Wconversion
        -Wno-missing-field-initializers
    )
endif()

install(TARGETS openfreedomfighters RUNTIME DESTINATION bin)
install(FILES assets/branding/openfreedomfighters-splash.bmp
        DESTINATION bin/assets)

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

    add_executable(off_picture_resource_tests tests/picture_resource_tests.cpp)
    target_link_libraries(off_picture_resource_tests PRIVATE off_core)
    add_test(NAME off_picture_resource_tests COMMAND off_picture_resource_tests)

    add_executable(off_picture_texture_binding_tests
        tests/picture_texture_binding_tests.cpp)
    target_link_libraries(off_picture_texture_binding_tests PRIVATE off_core)
    add_test(NAME off_picture_texture_binding_tests
        COMMAND off_picture_texture_binding_tests)

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

    add_executable(off_scene_archive_selection_tests
        tests/scene_archive_selection_tests.cpp)
    target_link_libraries(off_scene_archive_selection_tests PRIVATE off_core)
    target_compile_definitions(off_scene_archive_selection_tests PRIVATE
        OFF_TEST_WORK_DIR="${CMAKE_BINARY_DIR}/scene-archive-selection-work")
    add_test(NAME off_scene_archive_selection_tests
        COMMAND off_scene_archive_selection_tests)

    add_executable(off_zgf_bundle_tests tests/zgf_bundle_tests.cpp)
    target_link_libraries(off_zgf_bundle_tests PRIVATE off_core)
    add_test(NAME off_zgf_bundle_tests COMMAND off_zgf_bundle_tests)

    add_executable(off_graphics_settings_tests tests/graphics_settings_tests.cpp)
    target_link_libraries(off_graphics_settings_tests PRIVATE off_core)
    add_test(NAME off_graphics_settings_tests COMMAND off_graphics_settings_tests)

    add_executable(off_graphics_menu_draw_tests tests/graphics_menu_draw_tests.cpp)
    target_link_libraries(off_graphics_menu_draw_tests PRIVATE off_core)
    add_test(NAME off_graphics_menu_draw_tests COMMAND off_graphics_menu_draw_tests)

    add_executable(off_startup_graphics_composition_tests
        tests/startup_graphics_composition_tests.cpp)
    target_link_libraries(off_startup_graphics_composition_tests PRIVATE off_core)
    add_test(NAME off_startup_graphics_composition_tests
        COMMAND off_startup_graphics_composition_tests)

    add_executable(off_startup_graphics_asset_tests
        tests/startup_graphics_asset_tests.cpp)
    target_link_libraries(off_startup_graphics_asset_tests PRIVATE off_core)
    target_compile_definitions(off_startup_graphics_asset_tests PRIVATE
        OFF_TEST_WORK_DIR="${CMAKE_BINARY_DIR}/startup-graphics-asset-work")
    add_test(NAME off_startup_graphics_asset_tests
        COMMAND off_startup_graphics_asset_tests)

    add_executable(off_picture_transform_tests tests/picture_transform_tests.cpp)
    target_link_libraries(off_picture_transform_tests PRIVATE off_core)
    add_test(NAME off_picture_transform_tests COMMAND off_picture_transform_tests)

    add_executable(off_picture_expansion_tests tests/picture_expansion_tests.cpp)
    target_link_libraries(off_picture_expansion_tests PRIVATE off_core)
    add_test(NAME off_picture_expansion_tests COMMAND off_picture_expansion_tests)

    add_executable(off_startup_lifecycle_tests tests/startup_lifecycle_tests.cpp)
    target_link_libraries(off_startup_lifecycle_tests PRIVATE off_core)
    add_test(NAME off_startup_lifecycle_tests COMMAND off_startup_lifecycle_tests)

    add_executable(off_retail_ui_fonts_tests tests/retail_ui_fonts_tests.cpp)
    target_link_libraries(off_retail_ui_fonts_tests PRIVATE off_core)
    target_compile_definitions(off_retail_ui_fonts_tests PRIVATE
        OFF_TEST_WORK_DIR="${CMAKE_BINARY_DIR}/retail-ui-fonts-work")
    add_test(NAME off_retail_ui_fonts_tests COMMAND off_retail_ui_fonts_tests)

    add_executable(off_retail_ui_textures_tests tests/retail_ui_textures_tests.cpp)
    target_link_libraries(off_retail_ui_textures_tests PRIVATE off_core)
    add_test(NAME off_retail_ui_textures_tests COMMAND off_retail_ui_textures_tests)

    add_executable(off_simulation_runtime_tests tests/simulation_runtime_tests.cpp)
    target_link_libraries(off_simulation_runtime_tests PRIVATE off_core)
    add_test(NAME off_simulation_runtime_tests COMMAND off_simulation_runtime_tests)

    add_executable(off_simulation_world_tests tests/simulation_world_tests.cpp)
    target_link_libraries(off_simulation_world_tests PRIVATE off_core)
    add_test(NAME off_simulation_world_tests COMMAND off_simulation_world_tests)
endif()
