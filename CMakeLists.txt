cmake_minimum_required(VERSION 3.5.0)
project(RG-Projekat)

include_directories(glad/include)
add_executable(RG-Projekat main.cpp glad/src/glad.c)

# Intellisense for VS Code
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

# https://www.glfw.org/docs/3.3/build_guide.html#build_link_cmake_source
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw-3.3.4)
target_link_libraries(RG-Projekat glfw)

add_subdirectory(glm)
target_link_libraries(RG-Projekat glm)

