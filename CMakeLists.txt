cmake_minimum_required(VERSION 3.11)

project(oodle VERSION 0.1)

add_library(oodle STATIC)
add_executable(oodle1demo)
add_dependencies(oodle1demo oodle)

set_property(TARGET oodle PROPERTY POSITION_INDEPENDENT_CODE true)
target_compile_features(oodle PUBLIC cxx_std_17)
target_compile_options(oodle PUBLIC -Wall -Werror -Wextra)
target_include_directories(oodle PUBLIC include)

target_link_libraries(oodle1demo oodle)
target_compile_features(oodle1demo PUBLIC cxx_std_17)
target_compile_options(oodle1demo PUBLIC -Wall -Werror -Wextra)

add_subdirectory(src)

target_sources(oodle1demo PRIVATE ${PROJECT_SOURCE_DIR}/demo/demo.cpp ${PROJECT_SOURCE_DIR}/demo/Granny.cpp)
