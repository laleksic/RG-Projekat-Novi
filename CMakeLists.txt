cmake_minimum_required(VERSION 3.5.0)
project(RG-Projekat)

include_directories(glad/include)
add_executable(RG-Projekat main.cpp glad/src/glad.c)

find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)
find_package(assimp REQUIRED)
target_link_libraries(RG-Projekat glfw glm assimp)
