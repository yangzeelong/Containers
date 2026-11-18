# Shared CMake helpers for first-party targets.

# Stricter warnings for first-party targets only (never third-party targets).
# Warnings are errors so CI fails when a new warning sneaks in. Only the test
# and benchmark executables call this; the exported INTERFACE target is never
# touched, so downstream users are not forced into /WX or -Werror.
function(containers_enable_warnings TARGET)
    if(MSVC)
        target_compile_options(${TARGET} PRIVATE /W4 /WX)
    else()
        target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Werror)
    endif()
endfunction()
