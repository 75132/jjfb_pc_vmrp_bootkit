cmake_minimum_required(VERSION 3.20)
project(gwy_launcher C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(launcher_core STATIC
    src/launch_descriptor.c
)
target_include_directories(launcher_core PUBLIC include)

add_executable(gwy_launcher src/main.c)
target_link_libraries(gwy_launcher PRIVATE launcher_core)

enable_testing()
add_executable(test_smoke tests/test_smoke.c)
target_link_libraries(test_smoke PRIVATE launcher_core)
add_test(NAME smoke COMMAND test_smoke)
