if(NOT DEFINED TARGET_SYSTEM OR NOT DEFINED TARGET_PROCESSOR OR
   NOT DEFINED TARGET_POINTER_SIZE)
    message(FATAL_ERROR "Target system, processor, and pointer size are required.")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/ValidateNativeTarget.cmake")
off_validate_native_target(
    "${TARGET_SYSTEM}"
    "${TARGET_PROCESSOR}"
    "${TARGET_OSX_ARCHITECTURES}"
    "${TARGET_POINTER_SIZE}")
