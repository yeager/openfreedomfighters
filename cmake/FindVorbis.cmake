include(FindPackageHandleStandardArgs)

find_path(Vorbis_INCLUDE_DIR NAMES vorbis/vorbisfile.h vorbis/vorbisenc.h)
find_library(Vorbis_FILE_LIBRARY NAMES vorbisfile libvorbisfile.so.3)
find_library(Vorbis_ENCODER_LIBRARY NAMES vorbisenc libvorbisenc.so.2)
find_library(Vorbis_LIBRARY NAMES vorbis libvorbis.so.0)
find_library(Vorbis_OGG_LIBRARY NAMES ogg libogg.so.0)

find_package_handle_standard_args(
    Vorbis
    REQUIRED_VARS
        Vorbis_INCLUDE_DIR
        Vorbis_FILE_LIBRARY
        Vorbis_ENCODER_LIBRARY
        Vorbis_LIBRARY
        Vorbis_OGG_LIBRARY
)

if(Vorbis_FOUND AND NOT TARGET Vorbis::vorbisfile)
    add_library(Vorbis::vorbisfile UNKNOWN IMPORTED)
    set_target_properties(
        Vorbis::vorbisfile PROPERTIES
        IMPORTED_LOCATION "${Vorbis_FILE_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Vorbis_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${Vorbis_LIBRARY};${Vorbis_OGG_LIBRARY}"
    )
endif()

if(Vorbis_FOUND AND NOT TARGET Vorbis::vorbisenc)
    add_library(Vorbis::vorbisenc UNKNOWN IMPORTED)
    set_target_properties(
        Vorbis::vorbisenc PROPERTIES
        IMPORTED_LOCATION "${Vorbis_ENCODER_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Vorbis_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${Vorbis_LIBRARY};${Vorbis_OGG_LIBRARY}"
    )
endif()

mark_as_advanced(
    Vorbis_INCLUDE_DIR
    Vorbis_FILE_LIBRARY
    Vorbis_ENCODER_LIBRARY
    Vorbis_LIBRARY
    Vorbis_OGG_LIBRARY
)
