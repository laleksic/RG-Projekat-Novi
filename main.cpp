#include <stdio.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glfw/glfw3.h>

int main(int argc, char** argv) {
    glfwSetErrorCallback([](int code, const char *msg){
        fprintf(stdout, "GLFW error (%d): %s\n", code, msg);
        exit(1);
    });
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    GLFWwindow *window = glfwCreateWindow(640, 480, "RG-Projekat", 0, 0);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback([](GLenum source, GLenum type, GLuint id,
        GLenum severity, GLsizei length, const GLchar *message,
        const void *userParam) {
            fprintf(stdout, "GL debug message: %s\n", message);
            if (type == GL_DEBUG_TYPE_ERROR) {
                exit(1);
            }
    }, 0);

    glViewport(0,0,640,480);
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glfwSwapBuffers(window);
    }
    
    glfwDestroyWindow(window);
    glfwTerminate();
}
