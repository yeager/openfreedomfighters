cmake_minimum_required(VERSION 3.25)

project(OpenFreedomFighters VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
include(FetchContent)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Vorbis REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

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
    src/cutscene/timeline_position.cpp
    src/cutscene/command_pass.cpp
    src/cutscene/picture_fade.cpp
    src/cutscene/picture_activation_prefix.cpp
    src/audio/decode.cpp
    src/audio/sound_preferences.cpp
    src/audio/sound_records.cpp
    src/audio/sound_listener.cpp
    src/audio/sound_record_volume.cpp
    src/audio/stereo_source_command.cpp
    src/audio/stereo_stream_playback.cpp
    src/audio/intro_audio_stream.cpp
    src/runtime/application_clock.cpp
    src/runtime/component_lifecycle.cpp
    src/runtime/live_variables.cpp
    src/crypto/sha256.cpp
    src/data/archive_vfs.cpp
    src/data/audio_bank_header.cpp
    src/data/sound_definition_bank.cpp
    src/data/gms_image.cpp
    src/data/install.cpp
    src/data/install_manifest.cpp
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
    src/graphics/intro_prepared_resources.cpp
    src/graphics/intro_runtime.cpp
    src/graphics/intro_controller_initialization.cpp
    src/graphics/startup_graphics_prepared_plan.cpp
    src/graphics/startup_graphics_expanded_plan.cpp
    src/graphics/picture_transform.cpp
    src/graphics/picture_submission_cache.cpp
    src/graphics/fade_picture_size.cpp
    src/graphics/picture_expansion.cpp
    src/graphics/picture_material_state.cpp
    src/graphics/picture_draw_order.cpp
    src/graphics/picture_color_state.cpp
    src/graphics/picture_projection.cpp
    src/graphics/picture_view_parameters.cpp
    src/graphics/picture_view_transition.cpp
    src/graphics/picture_ordered_draw_loop.cpp
    src/graphics/picture_ordered_coordinator.cpp
    src/graphics/picture_preselection.cpp
    src/graphics/picture_draw_reset.cpp
    src/graphics/renderer_frame.cpp
    src/graphics/renderer_camera_registry.cpp
    src/graphics/intro_camera_state.cpp
    src/graphics/fresh_intro_camera.cpp
    src/graphics/preview_camera_update.cpp
    src/graphics/preview_translation.cpp
    src/graphics/preview_camera_component.cpp
    src/graphics/intro_picture_transform.cpp
    src/graphics/center_picture_position.cpp
    src/graphics/position_update_service.cpp
    src/graphics/picture_bounds.cpp
    src/graphics/group_bounds.cpp
    src/graphics/bounds_propagation.cpp
    src/graphics/admitted_view_pass.cpp
    src/graphics/renderer_frame_pass.cpp
    src/platform/startup_lifecycle.cpp
    src/platform/startup_preparation.cpp
    src/settings/graphics_settings.cpp
    src/simulation/fixed_step.cpp
    src/simulation/input.cpp
    src/simulation/world.cpp
    src/ui/graphics_menu.cpp
    src/ui/graphics_menu_draw.cpp
    src/ui/graphics_menu_pointer.cpp
    src/ui/retail_ui_fonts.cpp
    src/ui/retail_ui_textures.cpp
)
target_include_directories(off_core PUBLIC include)
target_compile_features(off_core PUBLIC cxx_std_23)
target_link_libraries(off_core PUBLIC Vorbis::vorbisfile ZLIB::ZLIB Threads::Threads)
set_target_properties(off_core PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
    target_compile_options(off_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(off_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

add_library(off_sdl_picture_clear STATIC src/platform/sdl_picture_clear.cpp)
target_link_libraries(off_sdl_picture_clear PUBLIC off_core ${OFF_SDL3_TARGET})
target_include_directories(off_sdl_picture_clear PRIVATE "${off_shader_assets_SOURCE_DIR}/examples")
if(MSVC)
    target_compile_options(off_sdl_picture_clear PRIVATE /W4 /permissive-)
else()
    target_compile_options(off_sdl_picture_clear PRIVATE -Wall -Wextra -Wpedantic -Wconversion
                           -Wno-missing-field-initializers)
endif()

add_executable(
    openfreedomfighters
    src/main.cpp
    src/platform/sdl_gpu_runtime.cpp
    src/platform/sdl_menu_gamepad.cpp
    src/platform/sdl_startup.cpp
)
add_library(off_sdl_picture_stage_shader STATIC src/platform/picture_stage_shader.cpp)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/picture_stage.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/generated/picture_stage.frag.hlsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/off/platform/generated/picture_stage_dxil.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/off/platform/generated/picture_stage_frag.hpp")
include(ShaderSourceHash)
off_shader_source_sha("${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/picture_stage.frag" OFF_PICTURE_SHADER_SOURCE_SHA)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/include/off/platform/generated/picture_stage_frag.hpp"
     OFF_PICTURE_SHADER_GENERATED_SHA REGEX "^// GLSL SHA256: ")
if(NOT OFF_PICTURE_SHADER_GENERATED_SHA STREQUAL "// GLSL SHA256: ${OFF_PICTURE_SHADER_SOURCE_SHA}")
    message(FATAL_ERROR "Picture shader source changed; regenerate with tools/build_picture_shader.py")
endif()
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/generated/picture_stage.frag.hlsl"
     OFF_PICTURE_HLSL_SOURCE_SHA REGEX "^// GLSL SHA256: ")
if(NOT OFF_PICTURE_HLSL_SOURCE_SHA STREQUAL "// GLSL SHA256: ${OFF_PICTURE_SHADER_SOURCE_SHA}")
    message(FATAL_ERROR "Picture HLSL source is stale; regenerate with tools/build_picture_shader.py")
endif()
off_shader_source_sha("${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/generated/picture_stage.frag.hlsl" OFF_PICTURE_HLSL_SHA)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/include/off/platform/generated/picture_stage_dxil.hpp"
     OFF_PICTURE_DXIL_SOURCE_SHA REGEX "^// HLSL SHA256: ")
if(NOT OFF_PICTURE_DXIL_SOURCE_SHA STREQUAL "// HLSL SHA256: ${OFF_PICTURE_HLSL_SHA}")
    message(FATAL_ERROR "Picture DXIL source is stale; regenerate with tools/build_picture_dxil.py")
endif()
target_link_libraries(off_sdl_picture_stage_shader PUBLIC off_core ${OFF_SDL3_TARGET})
if(MSVC)
    target_compile_options(off_sdl_picture_stage_shader PRIVATE /W4 /permissive-)
else()
    target_compile_options(off_sdl_picture_stage_shader PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()
add_library(off_sdl_intro_renderer STATIC src/platform/sdl_intro_renderer.cpp)
add_library(off_sdl_intro_presentation STATIC src/platform/sdl_intro_presentation.cpp)
target_link_libraries(off_sdl_intro_presentation PUBLIC off_sdl_picture_clear)
if(MSVC)
    target_compile_options(off_sdl_intro_presentation PRIVATE /W4 /permissive-)
else()
    target_compile_options(off_sdl_intro_presentation PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()
target_link_libraries(off_sdl_intro_renderer PUBLIC off_sdl_picture_stage_shader)
target_include_directories(off_sdl_intro_renderer PRIVATE "${off_shader_assets_SOURCE_DIR}/examples")
if(MSVC)
    target_compile_options(off_sdl_intro_renderer PRIVATE /W4 /permissive-)
else()
    target_compile_options(off_sdl_intro_renderer PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()
target_link_libraries(
    openfreedomfighters PRIVATE
    off_core off_sdl_picture_clear off_sdl_picture_stage_shader off_sdl_intro_renderer off_sdl_intro_presentation ${OFF_SDL3_TARGET} SDL3_ttf::SDL3_ttf
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
    add_executable(off_preview_keyboard_tests tests/preview_keyboard_tests.cpp)
    target_link_libraries(off_preview_keyboard_tests PRIVATE off_core)
    add_test(NAME off_preview_keyboard_tests COMMAND off_preview_keyboard_tests)
    add_executable(off_preview_translation_tests tests/preview_translation_tests.cpp)
    target_link_libraries(off_preview_translation_tests PRIVATE off_core)
    add_test(NAME off_preview_translation_tests COMMAND off_preview_translation_tests)
    add_executable(off_preview_camera_component_tests tests/preview_camera_component_tests.cpp)
    target_link_libraries(off_preview_camera_component_tests PRIVATE off_core)
    add_test(NAME off_preview_camera_component_tests COMMAND off_preview_camera_component_tests)
    add_executable(off_renderer_camera_registry_tests tests/renderer_camera_registry_tests.cpp)
    target_link_libraries(off_renderer_camera_registry_tests PRIVATE off_core)
    add_test(NAME off_renderer_camera_registry_tests COMMAND off_renderer_camera_registry_tests)
    add_executable(off_intro_audio_stream_tests tests/intro_audio_stream_tests.cpp)
    target_link_libraries(off_intro_audio_stream_tests PRIVATE off_core Vorbis::vorbisenc)
    add_test(NAME off_intro_audio_stream_tests COMMAND off_intro_audio_stream_tests)
    set_tests_properties(off_intro_audio_stream_tests PROPERTIES TIMEOUT 60)
    add_executable(off_vfs_file_reader_tests tests/vfs_file_reader_tests.cpp)
    target_link_libraries(off_vfs_file_reader_tests PRIVATE off_core)
    target_compile_definitions(off_vfs_file_reader_tests PRIVATE
        OFF_VFS_READER_TEST_WORK_DIR="${CMAKE_BINARY_DIR}/vfs-reader-test-work")
    add_test(NAME off_vfs_file_reader_tests COMMAND off_vfs_file_reader_tests)
    add_executable(off_sound_records_tests tests/sound_records_tests.cpp)
    target_link_libraries(off_sound_records_tests PRIVATE off_core)
    add_test(NAME off_sound_records_tests COMMAND off_sound_records_tests)
    add_executable(off_sound_record_volume_tests tests/sound_record_volume_tests.cpp)
    target_link_libraries(off_sound_record_volume_tests PRIVATE off_core)
    add_test(NAME off_sound_record_volume_tests COMMAND off_sound_record_volume_tests)
    add_executable(off_stereo_source_command_tests tests/stereo_source_command_tests.cpp)
    target_link_libraries(off_stereo_source_command_tests PRIVATE off_core)
    add_test(NAME off_stereo_source_command_tests COMMAND off_stereo_source_command_tests)
    add_executable(off_stereo_stream_playback_tests tests/stereo_stream_playback_tests.cpp)
    target_link_libraries(off_stereo_stream_playback_tests PRIVATE off_core Vorbis::vorbisenc)
    add_test(NAME off_stereo_stream_playback_tests COMMAND off_stereo_stream_playback_tests)
    set_tests_properties(off_stereo_stream_playback_tests PROPERTIES TIMEOUT 60)
    add_executable(off_sound_definition_bank_tests tests/sound_definition_bank_tests.cpp)
    target_link_libraries(off_sound_definition_bank_tests PRIVATE off_core)
    add_test(NAME off_sound_definition_bank_tests COMMAND off_sound_definition_bank_tests)
    add_executable(off_component_lifecycle_tests tests/component_lifecycle_tests.cpp)
    target_link_libraries(off_component_lifecycle_tests PRIVATE off_core)
    add_test(NAME off_component_lifecycle_tests COMMAND off_component_lifecycle_tests)
    add_executable(off_sound_preferences_tests tests/sound_preferences_tests.cpp)
    target_link_libraries(off_sound_preferences_tests PRIVATE off_core)
    add_test(NAME off_sound_preferences_tests COMMAND off_sound_preferences_tests)
    add_executable(off_application_clock_tests tests/application_clock_tests.cpp)
    target_link_libraries(off_application_clock_tests PRIVATE off_core)
    add_test(NAME off_application_clock_tests COMMAND off_application_clock_tests)
    add_test(NAME off_shader_source_hash_tests COMMAND ${CMAKE_COMMAND}
        "-DOFF_TEST_WORK_DIR=${CMAKE_BINARY_DIR}/test-work/shader-source-hash"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/shader_source_hash_tests.cmake")
    add_executable(off_intro_controller_initialization_tests tests/intro_controller_initialization_tests.cpp)
    add_executable(off_sdl_intro_presentation_tests tests/sdl_intro_presentation_tests.cpp)
    target_link_libraries(off_sdl_intro_presentation_tests PRIVATE off_sdl_intro_presentation)
    add_test(NAME off_sdl_intro_presentation_tests COMMAND off_sdl_intro_presentation_tests)
    set_tests_properties(off_sdl_intro_presentation_tests PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
    target_link_libraries(off_intro_controller_initialization_tests PRIVATE off_core)
    add_test(NAME off_intro_controller_initialization_tests COMMAND off_intro_controller_initialization_tests)
    add_executable(off_sdl_intro_renderer_tests tests/sdl_intro_renderer_tests.cpp)
    target_link_libraries(off_sdl_intro_renderer_tests PRIVATE off_sdl_intro_renderer)
    add_test(NAME off_sdl_intro_renderer_tests COMMAND off_sdl_intro_renderer_tests)
    set_tests_properties(off_sdl_intro_renderer_tests PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
    add_executable(off_sdl_picture_stage_shader_tests tests/sdl_picture_stage_shader_tests.cpp)
    target_link_libraries(off_sdl_picture_stage_shader_tests PRIVATE off_sdl_picture_stage_shader)
    target_include_directories(off_sdl_picture_stage_shader_tests PRIVATE "${off_shader_assets_SOURCE_DIR}/examples")
    add_test(NAME off_sdl_picture_stage_shader_tests COMMAND off_sdl_picture_stage_shader_tests)
    set_tests_properties(off_sdl_picture_stage_shader_tests PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
    add_executable(off_sdl_picture_clear_tests tests/sdl_picture_clear_tests.cpp)
    target_link_libraries(off_sdl_picture_clear_tests PRIVATE off_sdl_picture_clear)
    add_test(NAME off_sdl_picture_clear_tests COMMAND off_sdl_picture_clear_tests)
    set_tests_properties(off_sdl_picture_clear_tests PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
    add_executable(off_sdl_picture_depth_stencil_tests tests/sdl_picture_depth_stencil_tests.cpp)
    target_link_libraries(off_sdl_picture_depth_stencil_tests PRIVATE off_sdl_picture_clear)
    target_include_directories(off_sdl_picture_depth_stencil_tests PRIVATE "${off_shader_assets_SOURCE_DIR}/examples")
    add_test(NAME off_sdl_picture_depth_stencil_tests COMMAND off_sdl_picture_depth_stencil_tests)
    set_tests_properties(off_sdl_picture_depth_stencil_tests PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
    add_executable(off_core_tests tests/core_tests.cpp)
    target_link_libraries(off_core_tests PRIVATE off_core)
    target_compile_definitions(
        off_core_tests PRIVATE OFF_TEST_WORK_DIR="${CMAKE_BINARY_DIR}/test-work"
    )
    add_test(NAME off_core_tests COMMAND off_core_tests)
    add_executable(off_install_manifest_tests tests/install_manifest_tests.cpp)
    target_link_libraries(off_install_manifest_tests PRIVATE off_core)
    target_compile_definitions(off_install_manifest_tests PRIVATE
        OFF_MANIFEST_TEST_WORK_DIR="${CMAKE_BINARY_DIR}/install-manifest-test-work")
    add_test(NAME off_install_manifest_tests COMMAND off_install_manifest_tests)

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

    add_executable(off_cut_timeline_tests tests/cut_timeline_tests.cpp)
    target_link_libraries(off_cut_timeline_tests PRIVATE off_core)
    add_test(NAME off_cut_timeline_tests COMMAND off_cut_timeline_tests)

    add_executable(off_cut_command_pass_tests tests/cut_command_pass_tests.cpp)
    target_link_libraries(off_cut_command_pass_tests PRIVATE off_core)
    add_test(NAME off_cut_command_pass_tests COMMAND off_cut_command_pass_tests)

    add_executable(off_picture_fade_tests tests/picture_fade_tests.cpp)
    target_link_libraries(off_picture_fade_tests PRIVATE off_core)
    add_test(NAME off_picture_fade_tests COMMAND off_picture_fade_tests)

    add_executable(off_fade_picture_size_tests tests/fade_picture_size_tests.cpp)
    target_link_libraries(off_fade_picture_size_tests PRIVATE off_core)
    add_test(NAME off_fade_picture_size_tests COMMAND off_fade_picture_size_tests)

    add_executable(off_picture_color_state_tests tests/picture_color_state_tests.cpp)
    target_link_libraries(off_picture_color_state_tests PRIVATE off_core)
    add_test(NAME off_picture_color_state_tests COMMAND off_picture_color_state_tests)

    add_executable(off_intro_camera_state_tests tests/intro_camera_state_tests.cpp)
    target_link_libraries(off_intro_camera_state_tests PRIVATE off_core)
    add_test(NAME off_intro_camera_state_tests COMMAND off_intro_camera_state_tests)
    add_executable(off_camera_enabled_state_tests tests/camera_enabled_state_tests.cpp)
    target_link_libraries(off_camera_enabled_state_tests PRIVATE off_core)
    add_test(NAME off_camera_enabled_state_tests COMMAND off_camera_enabled_state_tests)
    add_executable(off_fresh_intro_camera_tests tests/fresh_intro_camera_tests.cpp)
    target_link_libraries(off_fresh_intro_camera_tests PRIVATE off_core)
    add_test(NAME off_fresh_intro_camera_tests COMMAND off_fresh_intro_camera_tests)
    add_executable(off_preview_camera_update_tests tests/preview_camera_update_tests.cpp)
    target_link_libraries(off_preview_camera_update_tests PRIVATE off_core)
    add_test(NAME off_preview_camera_update_tests COMMAND off_preview_camera_update_tests)
    add_executable(off_intro_picture_transform_tests tests/intro_picture_transform_tests.cpp)
    target_link_libraries(off_intro_picture_transform_tests PRIVATE off_core)
    add_test(NAME off_intro_picture_transform_tests COMMAND off_intro_picture_transform_tests)
    add_executable(off_center_picture_position_tests tests/center_picture_position_tests.cpp)
    target_link_libraries(off_center_picture_position_tests PRIVATE off_core)
    add_test(NAME off_center_picture_position_tests COMMAND off_center_picture_position_tests)
    add_executable(off_position_update_service_tests tests/position_update_service_tests.cpp)
    target_link_libraries(off_position_update_service_tests PRIVATE off_core)
    add_test(NAME off_position_update_service_tests COMMAND off_position_update_service_tests)
    add_executable(off_picture_bounds_tests tests/picture_bounds_tests.cpp)
    target_link_libraries(off_picture_bounds_tests PRIVATE off_core)
    add_test(NAME off_picture_bounds_tests COMMAND off_picture_bounds_tests)
    add_executable(off_picture_bounds_application_tests tests/picture_bounds_application_tests.cpp)
    target_link_libraries(off_picture_bounds_application_tests PRIVATE off_core)
    add_test(NAME off_picture_bounds_application_tests COMMAND off_picture_bounds_application_tests)
    add_executable(off_group_bounds_tests tests/group_bounds_tests.cpp)
    target_link_libraries(off_group_bounds_tests PRIVATE off_core)
    add_test(NAME off_group_bounds_tests COMMAND off_group_bounds_tests)
    add_executable(off_bounds_propagation_tests tests/bounds_propagation_tests.cpp)
    target_link_libraries(off_bounds_propagation_tests PRIVATE off_core)
    add_test(NAME off_bounds_propagation_tests COMMAND off_bounds_propagation_tests)

    add_executable(off_admitted_view_pass_tests tests/admitted_view_pass_tests.cpp)
    target_link_libraries(off_admitted_view_pass_tests PRIVATE off_core)
    add_test(NAME off_admitted_view_pass_tests COMMAND off_admitted_view_pass_tests)

    add_executable(off_renderer_frame_pass_tests tests/renderer_frame_pass_tests.cpp)
    target_link_libraries(off_renderer_frame_pass_tests PRIVATE off_core)
    add_test(NAME off_renderer_frame_pass_tests COMMAND off_renderer_frame_pass_tests)

    add_executable(off_picture_activation_prefix_tests tests/picture_activation_prefix_tests.cpp)
    target_link_libraries(off_picture_activation_prefix_tests PRIVATE off_core)
    add_test(NAME off_picture_activation_prefix_tests COMMAND off_picture_activation_prefix_tests)

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

    add_executable(off_graphics_menu_pointer_tests tests/graphics_menu_pointer_tests.cpp)
    target_link_libraries(off_graphics_menu_pointer_tests PRIVATE off_core)
    add_test(NAME off_graphics_menu_pointer_tests COMMAND off_graphics_menu_pointer_tests)

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

    add_executable(off_intro_prepared_resources_tests
        tests/intro_prepared_resources_tests.cpp)
    target_link_libraries(off_intro_prepared_resources_tests PRIVATE off_core)
    target_compile_definitions(off_intro_prepared_resources_tests PRIVATE
        OFF_TEST_WORK_DIR="${CMAKE_BINARY_DIR}/intro-prepared-resources-work")
    add_test(NAME off_intro_prepared_resources_tests
        COMMAND off_intro_prepared_resources_tests)

    add_executable(off_picture_transform_tests tests/picture_transform_tests.cpp)
    target_link_libraries(off_picture_transform_tests PRIVATE off_core)
    add_test(NAME off_picture_transform_tests COMMAND off_picture_transform_tests)

    add_executable(off_picture_submission_cache_tests tests/picture_submission_cache_tests.cpp)
    target_link_libraries(off_picture_submission_cache_tests PRIVATE off_core)
    add_test(NAME off_picture_submission_cache_tests COMMAND off_picture_submission_cache_tests)

    add_executable(off_picture_expansion_tests tests/picture_expansion_tests.cpp)
    target_link_libraries(off_picture_expansion_tests PRIVATE off_core)
    add_test(NAME off_picture_expansion_tests COMMAND off_picture_expansion_tests)

    add_executable(off_picture_material_state_tests tests/picture_material_state_tests.cpp)
    target_link_libraries(off_picture_material_state_tests PRIVATE off_core)
    add_test(NAME off_picture_material_state_tests COMMAND off_picture_material_state_tests)
    add_executable(off_picture_draw_order_tests tests/picture_draw_order_tests.cpp)
    target_link_libraries(off_picture_draw_order_tests PRIVATE off_core)
    add_test(NAME off_picture_draw_order_tests COMMAND off_picture_draw_order_tests)

    add_executable(off_picture_projection_tests tests/picture_projection_tests.cpp)
    target_link_libraries(off_picture_projection_tests PRIVATE off_core)
    add_test(NAME off_picture_projection_tests COMMAND off_picture_projection_tests)

    add_executable(off_picture_view_parameters_tests tests/picture_view_parameters_tests.cpp)
    target_link_libraries(off_picture_view_parameters_tests PRIVATE off_core)
    add_test(NAME off_picture_view_parameters_tests COMMAND off_picture_view_parameters_tests)
    add_executable(off_picture_view_transition_tests tests/picture_view_transition_tests.cpp)
    target_link_libraries(off_picture_view_transition_tests PRIVATE off_core)
    add_test(NAME off_picture_view_transition_tests COMMAND off_picture_view_transition_tests)
    add_executable(off_picture_ordered_draw_loop_tests tests/picture_ordered_draw_loop_tests.cpp)
    target_link_libraries(off_picture_ordered_draw_loop_tests PRIVATE off_core)
    add_test(NAME off_picture_ordered_draw_loop_tests COMMAND off_picture_ordered_draw_loop_tests)
    add_executable(off_picture_draw_reset_tests tests/picture_draw_reset_tests.cpp)
    target_link_libraries(off_picture_draw_reset_tests PRIVATE off_core)
    add_test(NAME off_picture_draw_reset_tests COMMAND off_picture_draw_reset_tests)
    add_executable(off_renderer_frame_tests tests/renderer_frame_tests.cpp)
    target_link_libraries(off_renderer_frame_tests PRIVATE off_core)
    add_test(NAME off_renderer_frame_tests COMMAND off_renderer_frame_tests)
    add_executable(off_picture_ordered_coordinator_tests tests/picture_ordered_coordinator_tests.cpp)
    target_link_libraries(off_picture_ordered_coordinator_tests PRIVATE off_core)
    add_test(NAME off_picture_ordered_coordinator_tests COMMAND off_picture_ordered_coordinator_tests)
    add_executable(off_picture_preselection_tests tests/picture_preselection_tests.cpp)
    target_link_libraries(off_picture_preselection_tests PRIVATE off_core)
    add_test(NAME off_picture_preselection_tests COMMAND off_picture_preselection_tests)

    add_executable(off_startup_lifecycle_tests tests/startup_lifecycle_tests.cpp)
    target_link_libraries(off_startup_lifecycle_tests PRIVATE off_core)
    add_test(NAME off_startup_lifecycle_tests COMMAND off_startup_lifecycle_tests)

    add_executable(off_startup_preparation_tests tests/startup_preparation_tests.cpp)
    target_link_libraries(off_startup_preparation_tests PRIVATE off_core)
    add_test(NAME off_startup_preparation_tests COMMAND off_startup_preparation_tests)

    add_executable(off_startup_window_tests
        tests/startup_window_tests.cpp src/platform/sdl_startup.cpp)
    target_link_libraries(off_startup_window_tests PRIVATE off_core ${OFF_SDL3_TARGET})
    add_test(NAME off_startup_window_tests COMMAND off_startup_window_tests)

    add_executable(off_sdl_menu_gamepad_tests
        tests/sdl_menu_gamepad_tests.cpp src/platform/sdl_menu_gamepad.cpp)
    target_link_libraries(off_sdl_menu_gamepad_tests PRIVATE off_core ${OFF_SDL3_TARGET})
    add_test(NAME off_sdl_menu_gamepad_tests COMMAND off_sdl_menu_gamepad_tests)
    set_tests_properties(off_sdl_menu_gamepad_tests PROPERTIES SKIP_RETURN_CODE 77)

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
