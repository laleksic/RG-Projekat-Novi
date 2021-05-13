#include <stdio.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glfw/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <array>
#include <algorithm>
#include <vector>
#include <string>
#include <memory>
#include <filesystem>
using namespace glm;
using namespace std;
namespace fs = filesystem;

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

class InputMaster {
    struct {
        array<bool, GLFW_KEY_LAST+1> KeyDown = {false};
        array<bool, GLFW_MOUSE_BUTTON_LAST+1> ButtonDown = {false};  
        vec2 MousePosition = vec2(0.0f);  
        ivec2 WindowSize = ivec2(0,0);
    } ThisFrame, LastFrame;

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
    void OnWindowSize(int width, int height) {
        ThisFrame.WindowSize = ivec2(width, height);
    }    
public:
    InputMaster(GLFWwindow *window) {
        glfwSetWindowUserPointer(window, this);
        glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode, int action, int mods){
            InputMaster *input = static_cast<InputMaster*>(glfwGetWindowUserPointer(window));
            input->OnKey(key, scancode, action, mods);
        });
        glfwSetMouseButtonCallback(window, [](GLFWwindow *window, int button, int action, int mods){
            InputMaster *input = static_cast<InputMaster*>(glfwGetWindowUserPointer(window));
            input->OnMouseButton(button, action, mods);
        });
        glfwSetCursorPosCallback(window, [](GLFWwindow *window, double xpos, double ypos){
            InputMaster *input = static_cast<InputMaster*>(glfwGetWindowUserPointer(window));
            input->OnCursorPos(xpos, ypos);
        });
        glfwSetWindowSizeCallback(window, [](GLFWwindow *window, int width, int height){
            InputMaster *input = static_cast<InputMaster*>(glfwGetWindowUserPointer(window));
            input->OnWindowSize(width, height);
        });   

        int w, h;
        glfwGetWindowSize(window, &w, &h);
        OnWindowSize(w, h);     
    }
    void NewFrame() {
        LastFrame = ThisFrame;
        glfwPollEvents();
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
    bool WasWindowResized() const {
        return ThisFrame.WindowSize != LastFrame.WindowSize;
    }
    ivec2 GetWindowSize() const {
        return ThisFrame.WindowSize;
    }
};

typedef shared_ptr<InputMaster> InputMasterPtr;

class FPSCamera: public Camera {
    InputMasterPtr Input;

public:
    FPSCamera(InputMasterPtr input): Input(input) {}
    void Update() {
        // Mouselook
        if (Input->IsButtonDown(GLFW_MOUSE_BUTTON_RIGHT)) {
            vec2 mouseDelta = Input->GetMouseDelta();
            const float MOUSE_SENSITIVITY = 0.1f;
            SetYaw(GetYaw() - mouseDelta.x * MOUSE_SENSITIVITY);
            SetPitch(GetPitch() - mouseDelta.y * MOUSE_SENSITIVITY);
        }
        
        // WASD Movement
        const float MOVEMENT_SPEED = 0.1f;
        vec3 forward = GetDirection();
        vec3 right = rotateY(vec3(forward.x, 0.0f, forward.z), radians(-90.0f));
        vec3 wishDirection(0.0f, 0.0f, 0.0f);
        wishDirection += (Input->IsKeyDown(GLFW_KEY_W)?1.0f:0.0f) * forward;
        wishDirection += (Input->IsKeyDown(GLFW_KEY_S)?-1.0f:0.0f) * forward;
        wishDirection += (Input->IsKeyDown(GLFW_KEY_A)?-1.0f:0.0f) * right;
        wishDirection += (Input->IsKeyDown(GLFW_KEY_D)?1.0f:0.0f) * right;
        SetPosition(GetPosition() + wishDirection * MOVEMENT_SPEED);
    }
};

class Engine {
    GLFWwindow *Window;

public:
    InputMasterPtr Input;
    Engine() {
        glfwSetErrorCallback([](int code, const char *msg){
            fprintf(stdout, "GLFW error (%d): %s\n", code, msg);
            abort();
        });
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
        Window = glfwCreateWindow(640, 480, "RG-Projekat", 0, 0);
        Input = make_shared<InputMaster>(Window);

        glfwMakeContextCurrent(Window);
        gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback([](GLenum source, GLenum type, GLuint id,
            GLenum severity, GLsizei length, const GLchar *message,
            const void *userParam) {
                fprintf(stdout, "GL debug message: %s\n", message);
                if (type == GL_DEBUG_TYPE_ERROR) {
                    abort();
                }
        }, 0);
    }
    ~Engine() {
        glfwDestroyWindow(Window);
        glfwTerminate();
    }
    void Run() {
        while (!glfwWindowShouldClose(Window)) {
            Input->NewFrame();       
            OnFrame(); 
            glfwSwapBuffers(Window);
        }
    }
    void Quit() {
        glfwSetWindowShouldClose(Window, GLFW_TRUE);
    }
    virtual void OnFrame() = 0;
};

class Texture {
    GLuint TextureID;

public:
    Texture(fs::path path) {
        glCreateTextures(GL_TEXTURE_2D, 1, &TextureID);
        int w, h;
        string pathString = path.string();
        // printf("Loading texture from %s\n", pathString.c_str());
        int channels;
        GLubyte *pixels = stbi_load(pathString.c_str(), &w, &h, &channels, 4);
        if (!pixels) {
            printf("Failed to open texture at %s\n", pathString.c_str());
            abort();
        }
        glTextureStorage2D(TextureID, 1, GL_RGBA8, w, h);
        glTextureSubImage2D(TextureID, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }
    ~Texture() {
        glDeleteTextures(1, &TextureID);
    }
    void Bind(GLuint unit) {
        glBindTextureUnit(unit, TextureID);
    }
};

typedef shared_ptr<Texture> TexturePtr;

class Mesh {
    GLuint VertexArray;

    // Attributes
    GLuint PositionBuffer;
    GLuint ColorBuffer;
    GLuint TexCoordBuffer;
    
    GLuint ElementBuffer;
    GLsizei ElementCount;

public:
    vector<vec3> Positions;
    vector<vec3> Colors;
    vector<vec2> TexCoords;
    vector<GLuint> Elements;

    Mesh() {
        glCreateVertexArrays(1, &VertexArray);
        glCreateBuffers(1, &ElementBuffer);

        // Attributes
        glCreateBuffers(1, &PositionBuffer);
        glCreateBuffers(1, &ColorBuffer);
        glCreateBuffers(1, &TexCoordBuffer);
        glVertexArrayAttribFormat(VertexArray, 0, 3, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribFormat(VertexArray, 1, 3, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribFormat(VertexArray, 2, 2, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(VertexArray, 0, 0);
        glVertexArrayAttribBinding(VertexArray, 1, 1);
        glVertexArrayAttribBinding(VertexArray, 2, 2);
        glEnableVertexArrayAttrib(VertexArray, 0);
        glEnableVertexArrayAttrib(VertexArray, 1);
        glEnableVertexArrayAttrib(VertexArray, 2);
        glVertexArrayVertexBuffer(VertexArray, 0, PositionBuffer, 0, sizeof(Positions[0]));
        glVertexArrayVertexBuffer(VertexArray, 1, ColorBuffer, 0, sizeof(Colors[0])); 
        glVertexArrayVertexBuffer(VertexArray, 2, TexCoordBuffer, 0, sizeof(TexCoords[0])); 

        glVertexArrayElementBuffer(VertexArray, ElementBuffer);    
    }   
    ~Mesh() {
        glDeleteVertexArrays(1, &VertexArray);
        glDeleteBuffers(1, &PositionBuffer);
        glDeleteBuffers(1, &ColorBuffer);
        glDeleteBuffers(1, &TexCoordBuffer);
    } 
    void UploadToGPU() {
        ElementCount = Elements.size();
        const size_t VERTEX_COUNT = Positions.size();
        
        // Sanity checks
        assert(Positions.size() == VERTEX_COUNT);
        assert(Colors.size() == VERTEX_COUNT);
        assert(TexCoords.size() == VERTEX_COUNT);
        assert(Elements.size() % 3 == 0);
        for (GLuint element: Elements) {
            assert(element < VERTEX_COUNT);
        }
        // ---

        // Attributes
        glNamedBufferData(PositionBuffer, VERTEX_COUNT * sizeof(Positions[0]), Positions.data(), GL_STATIC_DRAW);
        glNamedBufferData(ColorBuffer, VERTEX_COUNT * sizeof(Colors[0]), Colors.data(), GL_STATIC_DRAW);
        glNamedBufferData(TexCoordBuffer, VERTEX_COUNT * sizeof(TexCoords[0]), TexCoords.data(), GL_STATIC_DRAW);
        
        glNamedBufferData(ElementBuffer, ElementCount * sizeof(GLuint), Elements.data(), GL_STATIC_DRAW);
    }
    void Bind() {
        glBindVertexArray(VertexArray);
    }
    void Draw() {
        glDrawElements(GL_TRIANGLES, ElementCount, GL_UNSIGNED_INT, 0);
    }    
};

class Shader {
    GLuint Program = 0;

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
            abort();
        }
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &ok);
        if (ok == GL_FALSE) {
            GLsizei bufSize;
            glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &bufSize);
            GLchar buf[bufSize];
            glGetShaderInfoLog(fragmentShader, bufSize, 0, &buf[0]);
            printf("Fragment shader error: %s\n", buf);
            abort();
        }
        glGetProgramiv(Program, GL_LINK_STATUS, &ok);
        if (ok == GL_FALSE) {
            GLsizei bufSize;
            glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &bufSize);
            GLchar buf[bufSize];
            glGetProgramInfoLog(Program, bufSize, 0, &buf[0]);
            printf("Shader linking error: %s\n", buf);
            abort();
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
    void SetUniform(const char *name, GLint value) {
        glProgramUniform1i(Program, 
            glGetUniformLocation(Program, name),
            value
        );
    }    
    void Use() {
        glUseProgram(Program);
    }
};

typedef shared_ptr<Mesh> MeshPtr;
typedef shared_ptr<Shader> ShaderPtr;

class Main: public Engine {
    Mesh Triangle, Ground;
    TexturePtr Brick, Grass;
    ShaderPtr BasicShader;
    FPSCamera Camera;
    mat4 ProjectionMatrix;
    int Argc;
    char **Argv;

    void CalculateViewport() {
        ivec2 windowSize = Input->GetWindowSize();
        float aspectRatio = (float)windowSize.x / windowSize.y;
        ProjectionMatrix = perspective(radians(60.0f), aspectRatio, 0.1f, 10.0f);  
        glViewport(0,0, windowSize.x, windowSize.y);
    }

    fs::path GetExecutablePath() {
        // https://stackoverflow.com/a/55579815
        return fs::weakly_canonical(fs::path(Argv[0])).parent_path();
    }

    fs::path GetDataPath() {
        return GetExecutablePath()/"Data";
    }
public:
    Main(int argc, char **argv): Argc(argc), Argv(argv), Camera(Input) {
        Triangle.Positions = {
            vec3(-1.0f, -1.0f, 0.0f),
            vec3(1.0f, -1.0f, 0.0f),
            vec3(0.0f, 1.0f, 0.0f)
        };
        Triangle.Colors = {
            vec3(1.0f, 0.0f, 0.0f),
            vec3(0.0f, 1.0f, 0.0f),
            vec3(0.0f, 0.0f, 1.0f)
        };
        Triangle.Elements = {
            0, 1, 2
        };
        Triangle.TexCoords = {
            vec2(0,0),
            vec2(1,0),
            vec2(0.5,1)
        };
        Triangle.UploadToGPU();

        Ground.Positions = {
            vec3(-5,-1,-5),
            vec3(5,-1,-5),
            vec3(5,-1,5),
            vec3(-5,-1,5)
        };
        Ground.Colors = {
            vec3(1,1,1),
            vec3(1,1,1),
            vec3(1,1,1),
            vec3(1,1,1)
        };
        Ground.Elements = {0,1,2,  2,3,0};
        Ground.TexCoords = {
            vec2(0,0),
            vec2(1,0),
            vec2(1,1),
            vec2(0,1)
        };
        Ground.UploadToGPU();

        
        Brick = make_shared<Texture>(GetDataPath()/"brick.jpg");
        Grass = make_shared<Texture>(GetDataPath()/"grass.jpg");
        
        const char *shaderSource = R"glsl(
            uniform mat4 ModelViewProjection;
            uniform sampler2D DiffuseTexture;

            #if defined(VERTEX_SHADER)
                layout (location=0) in vec3 Position;
                layout (location=1) in vec3 Color;
                layout (location=2) in vec2 TexCoords;
                out VertexData {
                    vec3 Color;
                    vec2 TexCoords;
                } vertexData;

                void main() {
                    gl_Position.xyz = Position;
                    gl_Position.w = 1.0f;
                    gl_Position = ModelViewProjection * gl_Position;
                    vertexData.Color = Color;
                    vertexData.TexCoords = TexCoords;
                }
            #elif defined(FRAGMENT_SHADER)
                in VertexData {
                    vec3 Color;
                    vec2 TexCoords;
                } vertexData;

                out vec4 color;

                void main() {
                    color = texture(DiffuseTexture, vertexData.TexCoords);
                    color *= vec4(vertexData.Color, 1);
                }
            #endif
        )glsl";
        BasicShader = make_shared<Shader>(shaderSource);

        Camera.SetPosition(vec3(0.0f, 0.0f, 2.0f));  
        CalculateViewport();
    }
    virtual void OnFrame() override final {
        if (Input->WasWindowResized()) {
            CalculateViewport();
        }

        Camera.Update();

        mat4 modelViewProjectionMatrix = ProjectionMatrix * Camera.GetViewMatrix();
        BasicShader->SetUniform("ModelViewProjection", modelViewProjectionMatrix);

        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        BasicShader->Use();
        BasicShader->SetUniform("DiffuseTexture", 0);
        Brick->Bind(0);
        Triangle.Bind();
        Triangle.Draw();
        Grass->Bind(0);
        Ground.Bind();
        Ground.Draw();        
    }
};

int main(int argc, char** argv) {
    Main app(argc, argv);
    app.Run();
    return 0;
}
