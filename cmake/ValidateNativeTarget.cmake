# Validate the native release matrix before resolving dependencies. Keep this
# separate from the top-level build so the exact policy can be exercised with
# CMake script tests without configuring SDL or any host libraries.
# Script-mode CMake does not inherit the top-level minimum-version policy.
# `IN_LIST` below must therefore opt into its defined behavior explicitly.
if(POLICY CMP0057)
    cmake_policy(SET CMP0057 NEW)
endif()

function(off_validate_native_target target_system target_processor target_osx_architectures pointer_size)
    if("${pointer_size}" STREQUAL "" OR pointer_size LESS 8)
        message(FATAL_ERROR
            "OpenFreedomFighters requires a 64-bit target; configure an x86-64 or arm64 toolchain.")
    endif()

    if("${target_system}" STREQUAL "Windows")
        set(target_architectures "${target_processor}")
        set(supported_architectures amd64 x64 x86_64)
        set(platform_name "Windows")
    elseif("${target_system}" STREQUAL "Linux")
        set(target_architectures "${target_processor}")
        set(supported_architectures amd64 x86_64)
        set(platform_name "Linux and Steam Deck")
    elseif("${target_system}" STREQUAL "Darwin")
        if("${target_osx_architectures}" STREQUAL "")
            set(target_architectures "${target_processor}")
        else()
            set(target_architectures "${target_osx_architectures}")
        endif()
        set(supported_architectures amd64 x86_64 arm64 arm64e)
        set(platform_name "macOS")
    else()
        message(FATAL_ERROR
            "OpenFreedomFighters supports native Windows, Linux/Steam Deck, and macOS targets; got ${target_system}.")
    endif()

    if("${target_architectures}" STREQUAL "")
        message(FATAL_ERROR
            "OpenFreedomFighters could not determine the target architecture for ${platform_name}.")
    endif()

    foreach(target_architecture IN LISTS target_architectures)
        string(TOLOWER "${target_architecture}" normalized_architecture)
        if(NOT normalized_architecture IN_LIST supported_architectures)
            message(FATAL_ERROR
                "OpenFreedomFighters does not support ${target_architecture} on ${platform_name}. "
                "Supported target architectures: ${supported_architectures}.")
        endif()
    endforeach()
endfunction()
