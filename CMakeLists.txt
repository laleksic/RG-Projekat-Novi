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

# https://stackoverflow.com/questions/54048956/how-to-include-a-library-using-cmake-in-a-cross-platform-way
# https://github.com/assimp/assimp/blob/master/Build.md
# set(BUILD_SHARED_LIBS  ON)
# set(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
# set(ASSIMP_BUILD_TESTS OFF)
# add_subdirectory(assimp-5.0.1)
# target_link_libraries(RG-Projekat assimp)

add_subdirectory(glm)
target_link_libraries(RG-Projekat glm)

add_executable(assimp_test assimp_test.cpp)
#target_link_libraries(assimp_test assimp)
target_include_directories(assimp_test PRIVATE assimp-5.0.1/include )
target_include_directories(assimp_test PRIVATE assimp-5.0.1/build/include)
target_link_directories(assimp_test PRIVATE assimp-5.0.1/build/code)
target_link_directories(assimp_test PRIVATE assimp-5.0.1/build/contrib/IrrXML)
target_link_directories(assimp_test PRIVATE assimp-5.0.1/build/contrib/zlib)
target_link_libraries(assimp_test PRIVATE assimp gdi32)