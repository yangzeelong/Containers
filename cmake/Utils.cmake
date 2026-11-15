# Split an input list into "regular" files and files whose name ends with SUFFIX.
# Used to keep *Test.cpp / Benchmark*.cpp sources out of the library and collect
# them into the test / benchmark executables instead.
function(separate_files_by_suffix SUFFIX INPUT_LIST OUT_REGULAR_FILES OUT_MATCHED_FILES)
    set(local_regular)
    set(local_matched)

    string(REPLACE "." "\\." escaped_suffix "${SUFFIX}")

    foreach(current_file IN LISTS INPUT_LIST)
        if("${current_file}" MATCHES "${escaped_suffix}$")
            list(APPEND local_matched "${current_file}")
        else()
            list(APPEND local_regular "${current_file}")
        endif()
    endforeach()

    set(${OUT_REGULAR_FILES} ${local_regular} PARENT_SCOPE)
    set(${OUT_MATCHED_FILES} ${local_matched} PARENT_SCOPE)
endfunction()

# Stricter warnings for first-party targets only (never third-party targets).
function(containers_enable_warnings TARGET)
    if(MSVC)
        target_compile_options(${TARGET} PRIVATE /W4)
    else()
        target_compile_options(${TARGET} PRIVATE -Wall -Wextra)
    endif()
endfunction()
