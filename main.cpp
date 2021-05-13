#include <stdio.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glfw/glfw3.h>
#include <array>
using namespace glm;
using namespace std;

class Camera {
    // Pitch/Yaw are in degrees
    float Pitch = 0.0f;
    float Yaw = 0.0f;
    vec3 Position = vec3(0.0f, 0.0f, 0.0f);

public:
    void SetPitch(float pitch) {
        Pitch = pitch;
        const float EPSILON = 0.01f;
        if (Pitch > (90 - EPSILON))
            Pitch = 90 - EPSILON;
        if (Pitch < -(90 - EPSILON))
            Pitch = -(90 - EPSILON);
    };
    void SetYaw(float yaw) {
        Yaw = yaw;
        while (Yaw > 360) {
            Yaw -= 360;
        }
        while (Yaw < 0) {
            Yaw += 360;
        }
    }
    void SetPosition(vec3 position) {
        Position = position;
    }
    float GetPitch() const {
        return Pitch;
    };
    float GetYaw() const {
        return Yaw;
    };
    vec3 GetPosition() const {
        return Position;
    }
    vec3 GetDirection() const {
        vec3 direction(0.0f, 0.0f, -1.0f);
        direction = rotateY(direction, Yaw);
        direction = rotateX(direction, Pitch);
        return direction;
    }
    mat4 GetViewMatrix() const {
        const vec3 WORLD_UP(0.0f, 1.0f, 0.0f);
        return lookAt(Position, Position + GetDirection(), WORLD_UP);
    }
};

class Input {
    struct {
        array<bool, GLFW_KEY_LAST+1> KeyDown;
        array<bool, GLFW_MOUSE_BUTTON_LAST+1> ButtonDown;  
        vec2 MousePosition;    
    } ThisFrame, LastFrame;

public:
    void OnKey(int key, int scancode, int action, int mods) {
        switch (action) {
            case GLFW_PRESS: ThisFrame.KeyDown.at(key) = true; break;
            case GLFW_RELEASE: ThisFrame.KeyDown.at(key) = false; break;
        }
    }
    void OnMouseButton(int button, int action, int mods) {
        switch (action) {
            case GLFW_PRESS: ThisFrame.ButtonDown.at(button) = true; break;
            case GLFW_RELEASE: ThisFrame.ButtonDown.at(button) = false; break;            
        }
    }
    void OnCursorPos(double xpos, double ypos) {
        ThisFrame.MousePosition = vec2(xpos, ypos);
    }
    void NewFrame() {
        LastFrame = ThisFrame;
    }
    vec2 GetMousePosition() const {
        return ThisFrame.MousePosition;
    }
    vec2 GetMouseDelta() const {
        return ThisFrame.MousePosition - LastFrame.MousePosition;
    }
    bool IsKeyDown(int key) const {
        return ThisFrame.KeyDown.at(key);
    }
    bool IsButtonDown(int button) const {
        return ThisFrame.ButtonDown.at(button);
    }
    bool WasKeyPressed(int key) const {
        return ThisFrame.KeyDown.at(key) && !LastFrame.KeyDown.at(key);
    }
    bool WasKeyReleased(int key) const {
        return !ThisFrame.KeyDown.at(key) && LastFrame.KeyDown.at(key);
    }
    bool WasButtonPressed(int button) const {
        return ThisFrame.ButtonDown.at(button) && !LastFrame.ButtonDown.at(button);
    }
    bool WasButtonReleased(int button) const {
        return !ThisFrame.ButtonDown.at(button) && LastFrame.ButtonDown.at(button);
    }
};

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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow *window = glfwCreateWindow(640, 480, "RG-Projekat", 0, 0);
    Input input;
    glfwSetWindowUserPointer(window, &input);
    glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode, int action, int mods){
        Input *input = static_cast<Input*>(glfwGetWindowUserPointer(window));
        input->OnKey(key, scancode, action, mods);
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow *window, int button, int action, int mods){
        Input *input = static_cast<Input*>(glfwGetWindowUserPointer(window));
        input->OnMouseButton(button, action, mods);
    });
    glfwSetCursorPosCallback(window, [](GLFWwindow *window, double xpos, double ypos){
        Input *input = static_cast<Input*>(glfwGetWindowUserPointer(window));
        input->OnCursorPos(xpos, ypos);
    });
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

    struct {
        GLuint VertexArray;
        GLuint PositionBuffer;
        GLuint ColorBuffer;
    } triangle;

    GLfloat positions[] = {
        -1.0f, -1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        0.0f, 1.0f, 0.0f
    };

    GLfloat colors[] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f
    };

    glCreateVertexArrays(1, &triangle.VertexArray);
    glCreateBuffers(1, &triangle.PositionBuffer);
    glCreateBuffers(1, &triangle.ColorBuffer);
    glVertexArrayAttribFormat(triangle.VertexArray, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribFormat(triangle.VertexArray, 1, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(triangle.VertexArray, 0, 0);
    glVertexArrayAttribBinding(triangle.VertexArray, 1, 1);
    glEnableVertexArrayAttrib(triangle.VertexArray, 0);
    glEnableVertexArrayAttrib(triangle.VertexArray, 1);
    glVertexArrayVertexBuffer(triangle.VertexArray, 0, triangle.PositionBuffer, 0, sizeof(GLfloat) * 3);
    glVertexArrayVertexBuffer(triangle.VertexArray, 1, triangle.ColorBuffer, 0, sizeof(GLfloat) * 3);
    glNamedBufferData(triangle.PositionBuffer, 3*3*sizeof(GLfloat), positions, GL_STATIC_DRAW);
    glNamedBufferData(triangle.ColorBuffer, 3*3*sizeof(GLfloat), colors, GL_STATIC_DRAW);

    struct {
        GLuint VertexShader;
        GLuint FragmentShader;
        GLuint Program;

        // Uniforms
        GLuint ModelViewProjection;
    } shader;

    const char *shaderSource = R"glsl(
        uniform mat4 ModelViewProjection;

        #if defined(VERTEX_SHADER)
            layout (location=0) in vec3 Position;
            layout (location=1) in vec3 Color;
            out VertexData {
                vec3 Color;
            } vertexData;

            void main() {
                gl_Position.xyz = Position;
                gl_Position.w = 1.0f;
                gl_Position = ModelViewProjection * gl_Position;
                vertexData.Color = Color;
            }
        #elif defined(FRAGMENT_SHADER)
            in VertexData {
                vec3 Color;
            } vertexData;

            out vec4 color;

            void main() {
                color = vec4(vertexData.Color, 1.0f);    
            }
        #endif
    )glsl";
    const char *vertexSources[] = {"#version 450 core\n", "#define VERTEX_SHADER\n", shaderSource};
    const char *fragmentSources[] = {"#version 450 core\n", "#define FRAGMENT_SHADER\n", shaderSource};
    
    shader.VertexShader = glCreateShader(GL_VERTEX_SHADER);
    shader.FragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    shader.Program = glCreateProgram();
    glShaderSource(shader.VertexShader, 3, vertexSources, 0);
    glShaderSource(shader.FragmentShader, 3, fragmentSources, 0);
    glCompileShader(shader.VertexShader);
    glCompileShader(shader.FragmentShader);
    glAttachShader(shader.Program, shader.VertexShader);
    glAttachShader(shader.Program, shader.FragmentShader);
    glLinkProgram(shader.Program);
    shader.ModelViewProjection = glGetUniformLocation(shader.Program, "ModelViewProjection");

    GLint ok;
    glGetShaderiv(shader.VertexShader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE) {
        GLsizei bufSize;
        glGetShaderiv(shader.VertexShader, GL_INFO_LOG_LENGTH, &bufSize);
        GLchar buf[bufSize];
        glGetShaderInfoLog(shader.VertexShader, bufSize, 0, &buf[0]);
        printf("Vertex shader error: %s\n", buf);
        exit(1);
    }
    glGetShaderiv(shader.FragmentShader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE) {
        GLsizei bufSize;
        glGetShaderiv(shader.FragmentShader, GL_INFO_LOG_LENGTH, &bufSize);
        GLchar buf[bufSize];
        glGetShaderInfoLog(shader.FragmentShader, bufSize, 0, &buf[0]);
        printf("Fragment shader error: %s\n", buf);
        exit(1);
    }
    glGetProgramiv(shader.Program, GL_LINK_STATUS, &ok);
    if (ok == GL_FALSE) {
        GLsizei bufSize;
        glGetProgramiv(shader.Program, GL_INFO_LOG_LENGTH, &bufSize);
        GLchar buf[bufSize];
        glGetProgramInfoLog(shader.Program, bufSize, 0, &buf[0]);
        printf("Shader linking error: %s\n", buf);
        exit(1);
    }

    Camera camera;

    struct {
        mat4 Model, View, Projection;
        mat4 ModelViewProjection;
    } matrices;

    camera.SetPosition(vec3(0.0f, 0.0f, 2.0f));

    glViewport(0,0,640,480);
    glBindVertexArray(triangle.VertexArray);
    glUseProgram(shader.Program);
    while (!glfwWindowShouldClose(window)) {
        matrices.Model = mat4(1.0f);
        matrices.View = camera.GetViewMatrix();
        matrices.Projection = perspective(radians(60.0f), 640.0f/480.0f, 0.1f, 10.0f);
        matrices.ModelViewProjection = matrices.Projection * matrices.View * matrices.Model;
        glProgramUniformMatrix4fv(shader.Program, shader.ModelViewProjection, 1, GL_FALSE, value_ptr(matrices.ModelViewProjection));

        glfwPollEvents();
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glfwSwapBuffers(window);
    }
    
    glfwDestroyWindow(window);
    glfwTerminate();
}
