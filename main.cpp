#include <stdio.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glfw/glfw3.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
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
            fprintf(stderr, "GLFW error (%d): %s\n", code, msg);
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
                if (type == GL_DEBUG_TYPE_ERROR) {
                    fprintf(stderr, "GL error: %s\n", message);
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
        fprintf(stderr, "Loading texture from %s\n", pathString.c_str());
        int channels;
        GLubyte *pixels = stbi_load(pathString.c_str(), &w, &h, &channels, 4);
        if (!pixels) {
            fprintf(stderr, "Failed to open texture at %s\n", pathString.c_str());
            abort();
        }
        int levels = std::max(1, (int)log2((double)w));
        glTextureParameteri(TextureID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(TextureID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureStorage2D(TextureID, levels, GL_RGBA8, w, h);
        glTextureSubImage2D(TextureID, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glGenerateTextureMipmap(TextureID);
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
    GLuint NormalBuffer;
    GLuint TangentBuffer;
    GLuint BitangentBuffer;
    
    GLuint ElementBuffer;
    GLsizei ElementCount;

public:
    vector<vec3> Positions;
    vector<vec3> Colors;
    vector<vec2> TexCoords;
    vector<vec3> Normals;
    vector<vec3> Tangents;
    vector<vec3> Bitangents;
    vector<GLuint> Elements;

    Mesh() {
        glCreateVertexArrays(1, &VertexArray);
        glCreateBuffers(1, &ElementBuffer);

        // Attributes
        glCreateBuffers(1, &PositionBuffer);
        glCreateBuffers(1, &ColorBuffer);
        glCreateBuffers(1, &TexCoordBuffer);
        glCreateBuffers(1, &NormalBuffer);
        glCreateBuffers(1, &TangentBuffer);
        glCreateBuffers(1, &BitangentBuffer);
        glVertexArrayAttribFormat(VertexArray, 0, 3, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribFormat(VertexArray, 1, 3, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribFormat(VertexArray, 2, 2, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribFormat(VertexArray, 3, 3, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribFormat(VertexArray, 4, 3, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribFormat(VertexArray, 5, 3, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(VertexArray, 0, 0);
        glVertexArrayAttribBinding(VertexArray, 1, 1);
        glVertexArrayAttribBinding(VertexArray, 2, 2);
        glVertexArrayAttribBinding(VertexArray, 3, 3);
        glVertexArrayAttribBinding(VertexArray, 4, 3);
        glVertexArrayAttribBinding(VertexArray, 5, 3);
        glEnableVertexArrayAttrib(VertexArray, 0);
        glEnableVertexArrayAttrib(VertexArray, 1);
        glEnableVertexArrayAttrib(VertexArray, 2);
        glEnableVertexArrayAttrib(VertexArray, 3);
        glEnableVertexArrayAttrib(VertexArray, 4);
        glEnableVertexArrayAttrib(VertexArray, 5);
        glVertexArrayVertexBuffer(VertexArray, 0, PositionBuffer, 0, sizeof(Positions[0]));
        glVertexArrayVertexBuffer(VertexArray, 1, ColorBuffer, 0, sizeof(Colors[0])); 
        glVertexArrayVertexBuffer(VertexArray, 2, TexCoordBuffer, 0, sizeof(TexCoords[0])); 
        glVertexArrayVertexBuffer(VertexArray, 3, NormalBuffer, 0, sizeof(Normals[0])); 
        glVertexArrayVertexBuffer(VertexArray, 4, TangentBuffer, 0, sizeof(Tangents[0])); 
        glVertexArrayVertexBuffer(VertexArray, 5, BitangentBuffer, 0, sizeof(Bitangents[0])); 

        glVertexArrayElementBuffer(VertexArray, ElementBuffer);    
    }   
    ~Mesh() {
        glDeleteVertexArrays(1, &VertexArray);
        glDeleteBuffers(1, &PositionBuffer);
        glDeleteBuffers(1, &ColorBuffer);
        glDeleteBuffers(1, &TexCoordBuffer);
        glDeleteBuffers(1, &NormalBuffer);
        glDeleteBuffers(1, &TangentBuffer);
        glDeleteBuffers(1, &BitangentBuffer);
    } 
    void UploadToGPU() {
        ElementCount = Elements.size();
        const size_t VERTEX_COUNT = Positions.size();
        
        // Sanity checks
        assert(Positions.size() == VERTEX_COUNT);
        assert(Colors.size() == VERTEX_COUNT);
        assert(TexCoords.size() == VERTEX_COUNT);
        assert(Normals.size() == VERTEX_COUNT);
        assert(Tangents.size() == VERTEX_COUNT);
        assert(Bitangents.size() == VERTEX_COUNT);
        assert(Elements.size() % 3 == 0);
        for (GLuint element: Elements) {
            assert(element < VERTEX_COUNT);
        }
        // ---

        // Attributes
        glNamedBufferData(PositionBuffer, VERTEX_COUNT * sizeof(Positions[0]), Positions.data(), GL_STATIC_DRAW);
        glNamedBufferData(ColorBuffer, VERTEX_COUNT * sizeof(Colors[0]), Colors.data(), GL_STATIC_DRAW);
        glNamedBufferData(TexCoordBuffer, VERTEX_COUNT * sizeof(TexCoords[0]), TexCoords.data(), GL_STATIC_DRAW);
        glNamedBufferData(NormalBuffer, VERTEX_COUNT * sizeof(Normals[0]), Normals.data(), GL_STATIC_DRAW);
        glNamedBufferData(TangentBuffer, VERTEX_COUNT * sizeof(Tangents[0]), Tangents.data(), GL_STATIC_DRAW);
        glNamedBufferData(BitangentBuffer, VERTEX_COUNT * sizeof(Bitangents[0]), Bitangents.data(), GL_STATIC_DRAW);
        
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
    Shader(string source) {
        GLuint vertexShader;
        GLuint fragmentShader;
        const char *vertexSources[] = {"#version 450 core\n", "#define VERTEX_SHADER\n", source.c_str()};
        const char *fragmentSources[] = {"#version 450 core\n", "#define FRAGMENT_SHADER\n", source.c_str()};
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
            fprintf(stderr, "Vertex shader error: %s\n", buf);
            abort();
        }
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &ok);
        if (ok == GL_FALSE) {
            GLsizei bufSize;
            glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &bufSize);
            GLchar buf[bufSize];
            glGetShaderInfoLog(fragmentShader, bufSize, 0, &buf[0]);
            fprintf(stderr, "Fragment shader error: %s\n", buf);
            abort();
        }
        glGetProgramiv(Program, GL_LINK_STATUS, &ok);
        if (ok == GL_FALSE) {
            GLsizei bufSize;
            glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &bufSize);
            GLchar buf[bufSize];
            glGetProgramInfoLog(Program, bufSize, 0, &buf[0]);
            fprintf(stderr, "Shader linking error: %s\n", buf);
            abort();
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }
    ~Shader() {
        glDeleteProgram(Program);
    }
    void SetUniform(string name, const mat4& value) {
        glProgramUniformMatrix4fv(Program, 
            glGetUniformLocation(Program, name.c_str()),
            1, GL_FALSE, value_ptr(value)
        );
    }
    void SetUniform(string name, GLint value) {
        glProgramUniform1i(Program, 
            glGetUniformLocation(Program, name.c_str()),
            value
        );
    }    
    void SetUniform(string name, vec3 value) {
        glProgramUniform3fv(Program,
            glGetUniformLocation(Program, name.c_str()),
            1, value_ptr(value)
        );
    }
    void Use() {
        glUseProgram(Program);
    }
};

class IOUtils {
    fs::path ExecutablePath;

    bool TryFindDataFile(fs::path searchRoot, string fileName, fs::path& outPath) const {
        for (auto& entry: fs::directory_iterator(searchRoot)) {
            if (entry.is_regular_file() && entry.path().filename() == fileName) {
                outPath = entry.path();
                return true;
            }
            if (entry.is_directory() && TryFindDataFile(entry.path(), fileName, outPath)) {
                return true;
            }
        }
        return false;
    }    
public:
    IOUtils(fs::path executablePath): ExecutablePath(executablePath) {}
    fs::path GetExecutablePath() const {
        return ExecutablePath;
    }    
    fs::path GetDataPath() const {
        return GetExecutablePath()/"Data";
    }
    fs::path FindDataFile(string fileName) const {
        fileName = fs::path(fileName).filename().string();
        fs::path outPath;
        if (!TryFindDataFile(GetDataPath(), fileName, outPath)) {
            fprintf(stderr, "Can't open data file %s\n", fileName.c_str());
            abort();
        }
        return outPath;
    }    
    string LoadDataFileAsString(string fileName) const {
        fs::path path = FindDataFile(fileName);
        string pathString = path.string();
        string contents;
        FILE *fp = fopen(pathString.c_str(), "r");
        fseek(fp, 0, SEEK_END);
        contents.resize(ftell(fp));
        rewind(fp);
        fread(&contents[0], 1, contents.size(), fp);
        fclose(fp);
        return contents;
    }
};

class AssimpReadOnlyIOStream: public Assimp::IOStream {
    FILE *Fp;
public:
    AssimpReadOnlyIOStream(string path) {
        Fp = fopen(path.c_str(), "r");
        if (!Fp) {
            fprintf(stderr, "Failed to open %s\n", path.c_str());
            abort();
        }
    }
    void Close() {
        if (Fp)
            fclose(Fp);
        Fp = 0;
    }
    ~AssimpReadOnlyIOStream() {
        Close();
    }
    virtual size_t FileSize() const override {
        assert(Fp);
        fseek(Fp, 0, SEEK_END);
        long rv = ftell(Fp);
        rewind(Fp);
        return rv;
    }
    virtual void Flush() override { }
    virtual size_t Read(void *pvBuffer, size_t pSize, size_t pCount) override {
        assert(Fp);
        return fread(pvBuffer, pSize, pCount, Fp);
    }
    virtual aiReturn Seek(size_t pOffset, aiOrigin pOrigin) override {
        assert(Fp);
        int origin;
        switch (pOrigin) {
            case aiOrigin_CUR: origin = SEEK_CUR; break;
            case aiOrigin_END: origin = SEEK_END; break;
            case aiOrigin_SET: origin = SEEK_SET; break;
            default: abort();
        }
        return (aiReturn)fseek(Fp, pOffset, origin);
    }
    virtual size_t Tell() const override {
        assert(Fp);
        return ftell(Fp);
    }
    virtual size_t Write(const void *pvBuffer, size_t pSize, size_t pCount) override {
        return 0;
    }
};

typedef shared_ptr<IOUtils> IOUtilsPtr;


class AssimpReadOnlyIOSystem: public Assimp::IOSystem {
    IOUtilsPtr IO;
public:
    AssimpReadOnlyIOSystem(IOUtilsPtr io): IO(io) {}
    virtual void Close(Assimp::IOStream *pFile) override {
        //((AssimpReadOnlyIOStream*)pFile)->Close();
        delete pFile;
    }
    virtual bool ComparePaths (const char *one, const char *second) const override {
        fs::path p0(one);
        fs::path p1(second);
        return fs::equivalent(p0, p1);
    }
    virtual bool Exists (const char *pFile) const override {
        return fs::exists(fs::path(pFile));
    }
    virtual char getOsSeparator () const override {
        return fs::path::preferred_separator;
    }
    virtual Assimp::IOStream* Open (const char *pFile, const char *pMode="rb") {
        if (fs::path(pFile).is_absolute()) {
            return new AssimpReadOnlyIOStream(string(pFile));
        } 
        fs::path path = IO->FindDataFile(string(pFile));
        return new AssimpReadOnlyIOStream(path.string());
    }
};

class RandomNumberGenerator {
public:
    RandomNumberGenerator() {
        srand(time(0));
    }
    float RandomFloat() {
        return rand() / (float)RAND_MAX;
    }
    float RandomFloat(float lo, float hi) {
        return lo + RandomFloat() * (hi-lo);
    }
};

typedef shared_ptr<Mesh> MeshPtr;
typedef shared_ptr<Shader> ShaderPtr;

class Model {
public:
    vector<MeshPtr> Meshes;
    vector<TexturePtr> DiffuseTextures;
    vector<TexturePtr> SpecularTextures;
    vector<TexturePtr> NormalTextures;

    Model(fs::path path, IOUtilsPtr IO) {
        Assimp::Importer importer;
        //AssimpReadOnlyIOSystem ioHandler(IO);
        //importer.SetIOHandler(&ioHandler);
        string pathString = path.string();
        unsigned flags = 0;
        flags |= aiProcess_Triangulate;
        flags |= aiProcess_PreTransformVertices;
        flags |= aiProcess_FlipUVs;
        flags |= aiProcess_CalcTangentSpace;
        //flags |= aiProcess_GenNormals;
        // flags |= aiProcess_ForceGenNormals;
        // flags |= aiProcess_FixInfacingNormals;
        // flags |= aiProcess_FindInvalidData;
        // flags |= aiProcess_GenUVCoords;
        const aiScene *scene = importer.ReadFile(pathString.c_str(), flags);
        // scene = importer.ApplyPostProcessing(aiProcess_GenNormals);
        if (!scene) {
            fprintf(stderr, "Couldn't load %s!\n", pathString.c_str());
            abort();
        }
        for (int i=0; i<scene->mNumMeshes; ++i) {
            aiMesh *mesh = scene->mMeshes[i];
            if (!mesh->HasNormals()) {
                fprintf(stderr, "Malformed (no normals) mesh in %s\n", pathString.c_str());
                abort();
            }
            if (!mesh->HasPositions()) {
                fprintf(stderr, "Malformed (no positions) mesh in %s\n", pathString.c_str());
                abort();
            }
            if (!mesh->HasTextureCoords(0)) {
                fprintf(stderr, "Malformed (no texcoords) mesh in %s\n", pathString.c_str());
                abort();
            }
            if (!mesh->HasTangentsAndBitangents()) {
                fprintf(stderr, "Malformed (no tangents & bitangents) mesh in %s\n", pathString.c_str());
                abort();
            }
            MeshPtr meshp = make_shared<Mesh>();
            meshp->Positions.resize(mesh->mNumVertices);
            meshp->Colors.resize(mesh->mNumVertices);
            meshp->TexCoords.resize(mesh->mNumVertices);
            meshp->Normals.resize(mesh->mNumVertices);
            meshp->Tangents.resize(mesh->mNumVertices);
            meshp->Bitangents.resize(mesh->mNumVertices);

            meshp->Elements.reserve(mesh->mNumFaces * 3);
            for (int j=0; j<mesh->mNumVertices; ++j) {
                meshp->Positions[j] = vec3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);
                meshp->Colors[j] = vec3(1,1,1);
                meshp->TexCoords[j] = vec2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y);
                meshp->Normals[j] = vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z);
                meshp->Tangents[j] = vec3(mesh->mTangents[j].x, mesh->mTangents[j].y, mesh->mTangents[j].z);
                meshp->Bitangents[j] = vec3(mesh->mBitangents[j].x, mesh->mBitangents[j].y, mesh->mBitangents[j].z);
            }
            for (int j=0; j<mesh->mNumFaces; ++j) {
                meshp->Elements.push_back(mesh->mFaces[j].mIndices[0]);
                meshp->Elements.push_back(mesh->mFaces[j].mIndices[1]);
                meshp->Elements.push_back(mesh->mFaces[j].mIndices[2]);
            }
            meshp->UploadToGPU();
            Meshes.push_back(meshp);

            aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
            aiString diffuseMapPath, specularMapPath, normalMapPath;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &diffuseMapPath);
            material->GetTexture(aiTextureType_SPECULAR, 0, &specularMapPath);
            material->GetTexture(aiTextureType_NORMALS, 0, &normalMapPath);
            diffuseMapPath = (diffuseMapPath.length==0)?aiString("white.png"):diffuseMapPath;
            specularMapPath = (specularMapPath.length==0)?aiString("black.png"):specularMapPath;
            normalMapPath = (normalMapPath.length==0)?aiString("blankNormal.png"):normalMapPath;
            DiffuseTextures.push_back(make_shared<Texture>(IO->FindDataFile(diffuseMapPath.C_Str())));
            SpecularTextures.push_back(make_shared<Texture>(IO->FindDataFile(specularMapPath.C_Str())));
            NormalTextures.push_back(make_shared<Texture>(IO->FindDataFile(normalMapPath.C_Str())));
        }        
    }
};

class Light {
public:
    vec3 Position;
    vec3 Color;
};

typedef shared_ptr<Model> ModelPtr;

class Main: public Engine {
    ModelPtr Sponza;
    ShaderPtr BasicShader;
    FPSCamera Camera;
    mat4 ProjectionMatrix;
    int Argc;
    char **Argv;
    IOUtilsPtr IO;
    RandomNumberGenerator RNG;
    vector<Light> Lights;
    
    void CalculateViewport() {
        ivec2 windowSize = Input->GetWindowSize();
        float aspectRatio = (float)windowSize.x / windowSize.y;
        ProjectionMatrix = perspective(radians(60.0f), aspectRatio, 0.1f, 250.0f);  
        glViewport(0,0, windowSize.x, windowSize.y);
    }
    fs::path GetExecutablePath() const {
        // https://stackoverflow.com/a/55579815
        return fs::weakly_canonical(fs::path(Argv[0])).parent_path();
    }

public:
    Main(int argc, char **argv): Argc(argc), Argv(argv), Camera(Input) {
        IO = make_shared<IOUtils>(GetExecutablePath());
        // Sponza = make_shared<Model>(IO->FindDataFile("Sponza.gltf"), IO);
        Sponza = make_shared<Model>(IO->FindDataFile("sponza.obj"), IO);
        BasicShader = make_shared<Shader>(IO->LoadDataFileAsString("BasicShader.glsl"));

        Camera.SetPosition(vec3(0.0f, 0.0f, 2.0f));  
        CalculateViewport();

        const int LIGHT_COUNT = 32;
        for (int i=0; i<LIGHT_COUNT; ++i) {
            Light light;
            light.Position = vec3(
                RNG.RandomFloat(-7.5, 7.5),
                RNG.RandomFloat(0, 7.5),
                RNG.RandomFloat(-15, 5)
            );
            light.Color = vec3(
                RNG.RandomFloat(),
                RNG.RandomFloat(),
                RNG.RandomFloat()
            );
            Lights.push_back(light);
        }
    }
    virtual void OnFrame() override final {
        if (Input->WasWindowResized()) {
            CalculateViewport();
        }

        Camera.Update();

        mat4 modelMatrix = scale(vec3(0.01f));
        mat4 viewProjectionMatrix = ProjectionMatrix * Camera.GetViewMatrix();
        mat4 modelViewProjectionMatrix = viewProjectionMatrix * modelMatrix;
        BasicShader->SetUniform("ModelViewProjection", modelViewProjectionMatrix);
        BasicShader->SetUniform("Model", modelMatrix);
        BasicShader->SetUniform("CameraPosition", Camera.GetPosition());

        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        BasicShader->Use( );
        BasicShader->SetUniform("DiffuseTexture", 0);  
        BasicShader->SetUniform("SpecularTexture", 1);  
        BasicShader->SetUniform("NormalTexture", 2);  
        for (int i=0; i<Lights.size(); ++i) {
            BasicShader->SetUniform("Lights["+to_string(i)+"].Position", Lights[i].Position);
            BasicShader->SetUniform("Lights["+to_string(i)+"].Color", Lights[i].Color);
        }
        for (int i=0; i<Sponza->Meshes.size(); ++i) {
            Sponza->DiffuseTextures[i]->Bind(0);
            Sponza->SpecularTextures[i]->Bind(1);
            Sponza->NormalTextures[i]->Bind(2);
            Sponza->Meshes[i]->Bind();
            Sponza->Meshes[i]->Draw();
        }     
    }
};

int main(int argc, char** argv) {
    Main app(argc, argv);
    app.Run();
    return 0;
}
