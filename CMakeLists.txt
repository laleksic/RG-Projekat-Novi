cmake_minimum_required(VERSION 3.16.0)
project(RG-Projekat)

include_directories(glad/include)
include_directories(imgui-1.82)
include_directories(imgui-1.82/backends)
add_executable(RG-Projekat
        main.cpp
        glad/src/glad.c
        imgui-1.82/imgui.cpp
        imgui-1.82/imgui_draw.cpp
        imgui-1.82/imgui_tables.cpp
        imgui-1.82/imgui_widgets.cpp
        imgui-1.82/backends/imgui_impl_glfw.cpp
        imgui-1.82/backends/imgui_impl_opengl3.cpp
        )
set_property(TARGET RG-Projekat PROPERTY CXX_STANDARD 17)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(assimp REQUIRED)
target_link_libraries(RG-Projekat glfw glm assimp )
add_compile_definitions(IMGUI_IMPL_OPENGL_LOADER_GLAD)
if (WIN32)
    target_link_libraries(RG-Projekat imm32)
endif ()
if (UNIX)
    target_link_libraries(RG-Projekat dl GL)
endif ()

add_custom_target(DataFolder ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/Data
                ${CMAKE_CURRENT_BINARY_DIR}/Data)
