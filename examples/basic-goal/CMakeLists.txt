add_executable(example_basic_goal main.cpp)
containers_enable_warnings(example_basic_goal)
target_link_libraries(example_basic_goal PRIVATE containers)
target_compile_definitions(example_basic_goal PRIVATE
    CONTAINERS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
