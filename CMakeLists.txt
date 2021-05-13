cmake_minimum_required(VERSION 3.5.0)
project(RG-Projekat)

include_directories(glad/include)
add_executable(RG-Projekat main.cpp glad/src/glad.c)
set_property(TARGET RG-Projekat PROPERTY CXX_STANDARD 17)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(assimp REQUIRED)
target_link_libraries(RG-Projekat glfw glm assimp)

add_custom_command(
        TARGET RG-Projekat POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/Data
                ${CMAKE_CURRENT_BINARY_DIR}/Data)