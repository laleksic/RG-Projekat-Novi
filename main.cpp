#include <stdio.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glfw/glfw3.h>
#include <array>
#include <algorithm>
#include <vector>
#include <string>
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
        direction = rotateX(direction, radians(Pitch));
        direction = rotateY(direction, radians(Yaw));
        return direction;
    }
    mat4 GetViewMatrix() const {
        const vec3 WORLD_UP(0.0f, 1.0f, 0.0f);
        return lookAt(Position, Position + GetDirection(), WORLD_UP);
    }
};

class Input {
    struct {
        array<bool, GLFW_KEY_LAST+1> KeyDown = {false};
        array<bool, GLFW_MOUSE_BUTTON_LAST+1> ButtonDown = {false};  
        vec2 MousePosition = vec2(0.0f);    
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

class Mesh {
    GLuint VertexArray;

    // Attributes
    GLuint PositionBuffer;
    GLuint ColorBuffer;
    
    GLuint ElementBuffer;
    GLsizei ElementCount;

public:
    vector<vec3> Positions;
    vector<vec3> Colors;
    vector<GLuint> Elements;

    Mesh() {
        glCreateVertexArrays(1, &VertexArray);
        glCreateBuffers(1, &ElementBuffer);

        // Attributes
        glCreateBuffers(1, &PositionBuffer);
        glCreateBuffers(1, &ColorBuffer);
        glVertexArrayAttribFormat(VertexArray, 0, 3, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribFormat(VertexArray, 1, 3, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(VertexArray, 0, 0);
        glVertexArrayAttribBinding(VertexArray, 1, 1);
        glEnableVertexArrayAttrib(VertexArray, 0);
        glEnableVertexArrayAttrib(VertexArray, 1);
        glVertexArrayVertexBuffer(VertexArray, 0, PositionBuffer, 0, sizeof(Positions[0]));
        glVertexArrayVertexBuffer(VertexArray, 1, ColorBuffer, 0, sizeof(Colors[0])); 

        glVertexArrayElementBuffer(VertexArray, ElementBuffer);    
    }   
    ~Mesh() {
        glDeleteVertexArrays(1, &VertexArray);
        glDeleteBuffers(1, &PositionBuffer);
        glDeleteBuffers(1, &ColorBuffer);
    } 
    void UploadToGPU() {
        ElementCount = Elements.size();
        const size_t VERTEX_COUNT = Positions.size();
        
        // Sanity checks
        assert(Positions.size() == VERTEX_COUNT);
        assert(Colors.size() == VERTEX_COUNT);
        assert(Elements.size() % 3 == 0);
        for (GLuint element: Elements) {
            assert(element < VERTEX_COUNT);
        }
        // ---

        // Attributes
        glNamedBufferData(PositionBuffer, VERTEX_COUNT * sizeof(Positions[0]), Positions.data(), GL_STATIC_DRAW);
        glNamedBufferData(ColorBuffer, VERTEX_COUNT * sizeof(Colors[0]), Colors.data(), GL_STATIC_DRAW);
        
        glNamedBufferData(ElementBuffer, ElementCount * sizeof(GLuint), Elements.data(), GL_STATIC_DRAW);
    }
    void Draw() {
        glBindVertexArray(VertexArray);
        glDrawElements(GL_TRIANGLES, ElementCount, GL_UNSIGNED_INT, 0);
    }    
};

class Shader {
    GLuint Program;

public:
    Shader(const char *source) {
        GLuint vertexShader;
        GLuint fragmentShader;
        const char *vertexSources[] = {"#version 450 core\n", "#define VERTEX_SHADER\n", source};
        const char *fragmentSources[] = {"#version 450 core\n", "#define FRAGMENT_SHADER\n", source};
        vertexShader = glCreateShader(GL_VERTEX_SHADER);
        fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        Program = glCreateProgram();
        glShaderSource(vertexShader, 3, vertexSources, 0);
        glShaderSource(fragmentShader, 3, fragmentSources, 0);
        glCompileShader(vertexShader);
        glCompileShader(fragmentShader);
        glAttachShader(Program, vertexShader);
        glAttachShader(Program, fragmentShader);
        glLinkProgram(Program);

        GLint ok;
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &ok);
        if (ok == GL_FALSE) {
            GLsizei bufSize;
            glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &bufSize);
            GLchar buf[bufSize];
            glGetShaderInfoLog(vertexShader, bufSize, 0, &buf[0]);
            printf("Vertex shader error: %s\n", buf);
            exit(1);
        }
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &ok);
        if (ok == GL_FALSE) {
            GLsizei bufSize;
            glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &bufSize);
            GLchar buf[bufSize];
            glGetShaderInfoLog(fragmentShader, bufSize, 0, &buf[0]);
            printf("Fragment shader error: %s\n", buf);
            exit(1);
        }
        glGetProgramiv(Program, GL_LINK_STATUS, &ok);
        if (ok == GL_FALSE) {
            GLsizei bufSize;
            glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &bufSize);
            GLchar buf[bufSize];
            glGetProgramInfoLog(Program, bufSize, 0, &buf[0]);
            printf("Shader linking error: %s\n", buf);
            exit(1);
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }
    ~Shader() {
        glDeleteProgram(Program);
    }
    void SetUniform(const char *name, const mat4& value) {
        glProgramUniformMatrix4fv(Program, 
            glGetUniformLocation(Program, name),
            1, GL_FALSE, value_ptr(value)
        );
    }
    void Use() {
        glUseProgram(Program);
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

    Mesh *triangle = new Mesh;
    triangle->Positions = {
        vec3(-1.0f, -1.0f, 0.0f),
        vec3(1.0f, -1.0f, 0.0f),
        vec3(0.0f, 1.0f, 0.0f)
    };
    triangle->Colors = {
        vec3(1.0f, 0.0f, 0.0f),
        vec3(0.0f, 1.0f, 0.0f),
        vec3(0.0f, 0.0f, 1.0f)
    };
    triangle->Elements = {
        0, 1, 2
    };
    triangle->UploadToGPU();

    Mesh *ground = new Mesh;
    ground->Positions = {
        vec3(-5,-1,-5),
        vec3(5,-1,-5),
        vec3(5,-1,5),
        vec3(-5,-1,5)
    };
    ground->Colors = {
        vec3(1,1,1),
        vec3(1,1,1),
        vec3(1,1,1),
        vec3(1,1,1)
    };
    ground->Elements = {0,1,2,  2,3,0};
    ground->UploadToGPU();

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

    Shader shader(shaderSource);

    Camera camera;

    struct {
        mat4 Model, View, Projection;
        mat4 ModelViewProjection;
    } matrices;

    camera.SetPosition(vec3(0.0f, 0.0f, 2.0f));

    glViewport(0,0,640,480);
    shader.Use();
    while (!glfwWindowShouldClose(window)) {
        input.NewFrame();
        glfwPollEvents();

        if (input.IsButtonDown(GLFW_MOUSE_BUTTON_RIGHT)) {
            vec2 mouseDelta = input.GetMouseDelta();
            const float MOUSE_SENSITIVITY = 0.1f;
            camera.SetYaw(camera.GetYaw() - mouseDelta.x * MOUSE_SENSITIVITY);
            camera.SetPitch(camera.GetPitch() - mouseDelta.y * MOUSE_SENSITIVITY);
        }
        
        const float MOVEMENT_SPEED = 0.1f;
        vec3 forward = camera.GetDirection();
        vec3 right = rotateY(vec3(forward.x, 0.0f, forward.z), radians(-90.0f));
        vec3 wishDirection(0.0f, 0.0f, 0.0f);
        wishDirection += (input.IsKeyDown(GLFW_KEY_W)?1.0f:0.0f) * forward;
        wishDirection += (input.IsKeyDown(GLFW_KEY_S)?-1.0f:0.0f) * forward;
        wishDirection += (input.IsKeyDown(GLFW_KEY_A)?-1.0f:0.0f) * right;
        wishDirection += (input.IsKeyDown(GLFW_KEY_D)?1.0f:0.0f) * right;
        camera.SetPosition(camera.GetPosition() + wishDirection * MOVEMENT_SPEED);

        matrices.Model = mat4(1.0f);
        matrices.View = camera.GetViewMatrix();
        matrices.Projection = perspective(radians(60.0f), 640.0f/480.0f, 0.1f, 10.0f);
        matrices.ModelViewProjection = matrices.Projection * matrices.View * matrices.Model;
        shader.SetUniform("ModelViewProjection", matrices.ModelViewProjection);

        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        triangle->Draw();
        ground->Draw();
        glfwSwapBuffers(window);
    }
    
    glfwDestroyWindow(window);
    glfwTerminate();
}
