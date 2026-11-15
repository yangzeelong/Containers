file(GLOB_RECURSE CONTAINERS_ALL_FILES CONFIGURE_DEPENDS "**.cpp")

# Split sources and their colocated tests (*Test.cpp) so tests never land in the
# library, but still get compiled into the test executable.
separate_files_by_suffix("Test.cpp" "${CONTAINERS_ALL_FILES}"
    CONTAINERS_SRC_FILES CONTAINERS_TEST_FILES)

if(CONTAINERS_TEST_FILES)
    set_property(GLOBAL APPEND PROPERTY GLOBAL_TEST_SOURCES ${CONTAINERS_TEST_FILES})
endif()

include(GNUInstallDirs)

add_library(containers STATIC ${CONTAINERS_SRC_FILES})
containers_enable_warnings(containers)
target_include_directories(containers PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(containers PUBLIC yaml-cpp::yaml-cpp)

install(TARGETS containers
    EXPORT containersTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/containers
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
