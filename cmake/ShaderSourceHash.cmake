# Hash canonical LF text so Windows CRLF checkouts do not look like shader edits.
function(off_shader_source_sha path result)
    file(READ "${path}" shader_source)
    string(REPLACE "\r\n" "\n" shader_source "${shader_source}")
    string(SHA256 shader_sha "${shader_source}")
    set(${result} "${shader_sha}" PARENT_SCOPE)
endfunction()
