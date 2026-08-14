cmake_minimum_required(VERSION 3.20)

project(test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(test
    src/csv.c
    src/main.c
    src/matrix.c
    src/operations.c
)

target_include_directories(test PRIVATE include)

if(NOT MSVC)
    target_link_libraries(test PRIVATE m)
endif()
