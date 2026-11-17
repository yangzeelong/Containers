# Shared CMake helpers for first-party targets.

# Stricter warnings for first-party targets only (never third-party targets).
function(containers_enable_warnings TARGET)
    if(MSVC)
        target_compile_options(${TARGET} PRIVATE /W4)
    else()
        target_compile_options(${TARGET} PRIVATE -Wall -Wextra)
    endif()
endfunction()
