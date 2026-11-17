file(GLOB_RECURSE CONTAINERS_ALL_FILES CONFIGURE_DEPENDS "**.cpp")

# Split sources and their colocated tests (*Test.cpp) so tests never land in the
# library yet still get compiled into the test executable.
separate_files_by_suffix("Test.cpp" "${CONTAINERS_ALL_FILES}"
    CONTAINERS_SRC_FILES CONTAINERS_TEST_FILES)

if(CONTAINERS_TEST_FILES)
    set_property(GLOBAL APPEND PROPERTY GLOBAL_TEST_SOURCES ${CONTAINERS_TEST_FILES})
endif()

include(GNUInstallDirs)

# The common containers are header-only: an INTERFACE library holds the include
# directory and nothing else to build.
add_library(containers INTERFACE)
target_include_directories(containers INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(TARGETS containers EXPORT containersTargets)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/containers
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
