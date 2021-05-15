#define STB_IMAGE_IMPLEMENTATION
#include "main.hpp"

class Engine {
    GLFWwindow *Window;
    ImGuiContext *Gui;
    bool FirstFrame = true;

    // Input handling
    // --------------
    struct {
        array<bool, GLFW_KEY_LAST+1> KeyDown = {false};
        array<bool, GLFW_MOUSE_BUTTON_LAST+1> ButtonDown = {false};  
        vec2 MousePosition = vec2(0.0f);  
        ivec2 WindowSize = ivec2(0,0);
    } ThisFrame, LastFrame;

    void Begin() {
        LastFrame = ThisFrame;
        glfwPollEvents(); 

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();            
        ImGui::Begin("RG-Projekat");
    }
    void End() {
        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(Window);
    }
public:
    Engine() {
        // Init glfw/gl
        // ------------
        glfwSetErrorCallback([](int code, const char *msg){
            cerr << "GLFW error (" << code << ")" << msg << endl;
            abort();
        });
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
        Window = glfwCreateWindow(640, 480, "RG-Projekat", 0, 0);

        glfwMakeContextCurrent(Window);
        gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback([](GLenum source, GLenum type, GLuint id,
            GLenum severity, GLsizei length, const GLchar *message,
            const void *userParam) {
                if (type == GL_DEBUG_TYPE_ERROR) {
                    cerr << "GL error: " << message << endl;
                    abort();
                }
        }, 0);

        glfwSetKeyCallback(Window, [](GLFWwindow *window, int key, int scancode, int action, int mods){
            switch (action) {
                case GLFW_PRESS: TheEngine->ThisFrame.KeyDown.at(key) = true; break;
                case GLFW_RELEASE: TheEngine->ThisFrame.KeyDown.at(key) = false; break;
            }
        });
        glfwSetMouseButtonCallback(Window, [](GLFWwindow *window, int button, int action, int mods){
            switch (action) {
                case GLFW_PRESS: TheEngine->ThisFrame.ButtonDown.at(button) = true; break;
                case GLFW_RELEASE: TheEngine->ThisFrame.ButtonDown.at(button) = false; break;            
            }
        });
        glfwSetCursorPosCallback(Window, [](GLFWwindow *window, double xpos, double ypos){
            TheEngine->ThisFrame.MousePosition = vec2(xpos, ypos);
        });
        glfwSetWindowSizeCallback(Window, [](GLFWwindow *window, int width, int height){
            TheEngine->ThisFrame.WindowSize = ivec2(width, height);
        });   

        int w, h;
        glfwGetWindowSize(Window, &w, &h);
        ThisFrame.WindowSize = ivec2(w, h);     

        // https://blog.conan.io/2019/06/26/An-introduction-to-the-Dear-ImGui-library.html
        // Init imgui
        // ----------
        IMGUI_CHECKVERSION();
        Gui = ImGui::CreateContext();
        ImGui_ImplGlfw_InitForOpenGL(Window, true);
        ImGui_ImplOpenGL3_Init("#version 450 core");
        ImGui::StyleColorsDark();          
    }
    ~Engine() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(Gui);
        glfwDestroyWindow(Window);
        glfwTerminate();
    }
    bool Run() {
        if (!FirstFrame)
            End();
        FirstFrame = false;
        Begin();
        if (glfwWindowShouldClose(Window)) {
            End();
            return false;
        }
        return true;
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

class Camera {
    // Pitch/Yaw are in degrees
    float Pitch = 0.0f;
    float Yaw = 0.0f;
    vec3 Position = vec3(0.0f, 0.0f, 0.0f);

public:
    void SetPitch(float pitch) {
        Pitch = pitch;
        const float EPSILON = 0.1f;
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

class FPSCamera: public Camera {
public:
    void Update() {
        // Mouselook
        if (TheEngine->IsButtonDown(GLFW_MOUSE_BUTTON_RIGHT)) {
            vec2 mouseDelta = TheEngine->GetMouseDelta();
            const float MOUSE_SENSITIVITY = 0.1f;
            SetYaw(GetYaw() - mouseDelta.x * MOUSE_SENSITIVITY);
            SetPitch(GetPitch() - mouseDelta.y * MOUSE_SENSITIVITY);
        }
        
        // WASD Movement
        const float MOVEMENT_SPEED = 0.1f;
        vec3 forward = GetDirection();
        vec3 right = rotateY(vec3(forward.x, 0.0f, forward.z), radians(-90.0f));
        vec3 wishDirection(0.0f, 0.0f, 0.0f);
        wishDirection += (TheEngine->IsKeyDown(GLFW_KEY_W)?1.0f:0.0f) * forward;
        wishDirection += (TheEngine->IsKeyDown(GLFW_KEY_S)?-1.0f:0.0f) * forward;
        wishDirection += (TheEngine->IsKeyDown(GLFW_KEY_A)?-1.0f:0.0f) * right;
        wishDirection += (TheEngine->IsKeyDown(GLFW_KEY_D)?1.0f:0.0f) * right;
        if (length(wishDirection) > 0.001f)
            wishDirection = normalize(wishDirection);
        SetPosition(GetPosition() + wishDirection * MOVEMENT_SPEED);
    }
};

class Texture {
    GLuint TextureID;
    bool HasAlphaChannel = false;

public:
    Texture(string path) {
        glCreateTextures(GL_TEXTURE_2D, 1, &TextureID);
        int w, h;
        cerr << "Loading texture from " << path << endl;
        int channels;
        GLubyte *pixels = stbi_load(path.c_str(), &w, &h, &channels, 4);
        if (!pixels) {
            cerr << "Failed!" << endl;
            abort();
        }
        if (channels == 4)
            HasAlphaChannel = true;
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
    bool ShouldAlphaClip() const { return HasAlphaChannel; }
};


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
        // Koja sam ja budala...
        glVertexArrayAttribBinding(VertexArray, 4, 4);
        glVertexArrayAttribBinding(VertexArray, 5, 5);
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
    Shader(string path) {
        string source;
        FILE *fp = fopen(path.c_str(), "r");
        fseek(fp, 0, SEEK_END);
        source.resize(ftell(fp));
        rewind(fp);
        fread(&source[0], 1, source.size(), fp);
        fclose(fp);

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
            cerr << "Vertex shader error: " << buf << endl;
            abort();
        }
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &ok);
        if (ok == GL_FALSE) {
            GLsizei bufSize;
            glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &bufSize);
            GLchar buf[bufSize];
            glGetShaderInfoLog(fragmentShader, bufSize, 0, &buf[0]);
            cerr << "Fragment shader error: " << buf << endl;
            abort();
        }
        glGetProgramiv(Program, GL_LINK_STATUS, &ok);
        if (ok == GL_FALSE) {
            GLsizei bufSize;
            glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &bufSize);
            GLchar buf[bufSize];
            glGetProgramInfoLog(Program, bufSize, 0, &buf[0]);
            cerr << "Shader linking error: " << buf << endl;
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
    void SetUniform(string name, bool value) {
        glProgramUniform1i(Program, 
            glGetUniformLocation(Program, name.c_str()),
            value?GL_TRUE:GL_FALSE
        );
    }         
    void SetUniform(string name, float value) {
        glProgramUniform1f(Program, 
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

TexturePtr LoadTexture(string path);

class Model {
public:
    vector<MeshPtr> Meshes;
    vector<TexturePtr> DiffuseTextures;
    vector<TexturePtr> SpecularTextures;
    vector<TexturePtr> NormalTextures;
    vector<TexturePtr> BumpTextures;

    Model(string path) {
        Assimp::Importer importer;
        unsigned flags = 0;
        flags |= aiProcess_Triangulate;
        flags |= aiProcess_PreTransformVertices;
        flags |= aiProcess_FlipUVs;
        flags |= aiProcess_FixInfacingNormals;
        flags |= aiProcess_FindInvalidData;
        const aiScene *scene = importer.ReadFile(path.c_str(), flags);
        scene = importer.ApplyPostProcessing(aiProcess_GenNormals | aiProcess_CalcTangentSpace);
        if (!scene) {
            cerr << "Couldn't load " << path << endl;
            abort();
        }
        for (int i=0; i<scene->mNumMeshes; ++i) {
            aiMesh *mesh = scene->mMeshes[i];

            assert(mesh->HasNormals());
            assert(mesh->HasPositions());
            assert(mesh->HasTextureCoords(0));
            assert(mesh->HasTangentsAndBitangents());

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
            aiString diffuseMapPath, specularMapPath, normalMapPath, bumpMapPath;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &diffuseMapPath);
            material->GetTexture(aiTextureType_SPECULAR, 0, &specularMapPath);
            material->GetTexture(aiTextureType_NORMALS, 0, &normalMapPath);
            material->GetTexture(aiTextureType_HEIGHT, 0, &bumpMapPath);
            diffuseMapPath = (diffuseMapPath.length==0)?aiString("Data/textures/white.png"):diffuseMapPath;
            specularMapPath = (specularMapPath.length==0)?aiString("Data/textures/black.png"):specularMapPath;
            normalMapPath = (normalMapPath.length==0)?aiString("Data/textures/blankNormal.png"):normalMapPath;
            bumpMapPath = (bumpMapPath.length==0)?aiString("Data/textures/black.png"):bumpMapPath;
            DiffuseTextures.push_back(LoadTexture(diffuseMapPath.C_Str()));
            SpecularTextures.push_back(LoadTexture(specularMapPath.C_Str()));
            NormalTextures.push_back(LoadTexture(normalMapPath.C_Str()));
            BumpTextures.push_back(LoadTexture(bumpMapPath.C_Str()));
        }        
    }
};

class Light {
public:
    vec3 Position;
    vec3 Color;
};

ModelPtr LoadModel(string path) {
    return make_shared<Model>(path);
}
TexturePtr LoadTexture(string path) {
    // Cache textures here so we don't load same texture many times over
    static map<string, TexturePtr> LoadedTextures;

    auto it = LoadedTextures.find(path);
    if (it == LoadedTextures.end()) {
        TexturePtr texture = make_shared<Texture>(path);
        LoadedTextures[path] = texture;
        return texture;
    }
    return it->second;
}
ShaderPtr LoadShader(string path) {
    return make_shared<Shader>(path);
}
float RandomFloat() {
    return rand() / (float)RAND_MAX;
}
float RandomFloat(float lo, float hi) {
    return lo + RandomFloat() * (hi-lo);
}

ModelPtr Sponza, Cube;
ShaderPtr BasicShader, LightCubeShader;
FPSCamera Camera;
mat4 ProjectionMatrix;
vector<Light> Lights[2];
float lightLerp = 0.0f;

float ParallaxDepth = 0.04f;

void CalculateViewportAndProjectionMatrix() {
    ivec2 windowSize = TheEngine->GetWindowSize();
    float aspectRatio = (float)windowSize.x / windowSize.y;
    ProjectionMatrix = perspective(radians(60.0f), aspectRatio, 0.1f, 250.0f);  
    glViewport(0,0, windowSize.x, windowSize.y);
}

int main(int argc, char** argv) {
    TheEngine = make_shared<Engine>();

    srand(time(0));
    Sponza = LoadModel("Data/models/sponza.obj");
    Cube = LoadModel("Data/models/cube.obj");
    BasicShader = LoadShader("Data/shaders/BasicShader.glsl");
    LightCubeShader = LoadShader("Data/shaders/LightCube.glsl");

    Camera.SetPosition(vec3(0.0f, 2.0f, 2.0f));  
    CalculateViewportAndProjectionMatrix();

    const int LIGHT_COUNT = 32;
    for (int j=0; j<2; ++j) {
        for (int i=0; i<LIGHT_COUNT; ++i) {
            Light light;
            light.Position = vec3(
                RandomFloat(-7.5, 7.5),
                RandomFloat(0, 7.5),
                RandomFloat(-15, 5)
            );
            light.Color = vec3(
                RandomFloat(),
                RandomFloat(),
                RandomFloat()
            );
            Lights[j].push_back(light);
        }
    }

    while (TheEngine->Run()) {
        if (TheEngine->WasWindowResized()) {
            CalculateViewportAndProjectionMatrix();
        }

        Camera.Update();

        mat4 modelMatrix = scale(vec3(0.01f));
        mat4 viewProjectionMatrix = ProjectionMatrix * Camera.GetViewMatrix();
        mat4 modelViewProjectionMatrix = viewProjectionMatrix * modelMatrix;
        BasicShader->SetUniform("MVPMat", modelViewProjectionMatrix);
        BasicShader->SetUniform("ModelMat", modelMatrix);
        BasicShader->SetUniform("CameraPosition", Camera.GetPosition());

        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        ImGui::DragFloat("ParallaxDepth",&ParallaxDepth,
            0.01f, 0, 0.2f, "%f", 1.0f);     
        BasicShader->SetUniform("ParallaxDepth", ParallaxDepth);

        BasicShader->Use( );
        BasicShader->SetUniform("DiffuseTexture", 0);  
        BasicShader->SetUniform("SpecularTexture", 1);  
        BasicShader->SetUniform("NormalTexture", 2);  
        BasicShader->SetUniform("BumpTexture", 3);  
        for (int i=0; i<Lights[0].size(); ++i) {
            vec3 lightPosition = lerp(Lights[0][i].Position, Lights[1][i].Position, smoothstep(0.0f,1.0f,lightLerp));
            vec3 lightColor = lerp(Lights[0][i].Color, Lights[1][i].Color, smoothstep(0.0f,1.0f,lightLerp));
            BasicShader->SetUniform("Lights["+to_string(i)+"].Position", lightPosition);
            BasicShader->SetUniform("Lights["+to_string(i)+"].Color", lightColor);
        }
        BasicShader->SetUniform("AmbientLight", vec3(0.075,0.075,0.125));
        lightLerp = (sin(glfwGetTime())+1.0)/2.0;

        for (int i=0; i<Sponza->Meshes.size(); ++i) {
            if (Sponza->DiffuseTextures[i]->ShouldAlphaClip()) {
                glDisable(GL_CULL_FACE);
                BasicShader->SetUniform("Translucent", true);
            } else {
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
                BasicShader->SetUniform("Translucent", false);
            }
            Sponza->DiffuseTextures[i]->Bind(0);
            Sponza->SpecularTextures[i]->Bind(1);
            Sponza->NormalTextures[i]->Bind(2);
            Sponza->BumpTextures[i]->Bind(3);
            Sponza->Meshes[i]->Bind();
            Sponza->Meshes[i]->Draw();
        }

        LightCubeShader->Use();
        for (int i=0; i<Lights[0].size(); ++i) {
            vec3 lightPosition = lerp(Lights[0][i].Position, Lights[1][i].Position, smoothstep(0.0f,1.0f,lightLerp));
            vec3 lightColor = lerp(Lights[0][i].Color, Lights[1][i].Color, smoothstep(0.0f,1.0f,lightLerp));
            modelMatrix = translate(lightPosition) * scale(vec3(0.125f));
            modelViewProjectionMatrix = viewProjectionMatrix * modelMatrix;
            LightCubeShader->SetUniform("MVPMat", modelViewProjectionMatrix);
            LightCubeShader->SetUniform("ModelMat", modelMatrix);
            LightCubeShader->SetUniform("LightColor", lightColor);
            for (int j=0; j<Cube->Meshes.size(); ++j) {
                Cube->Meshes[j]->Bind();
                Cube->Meshes[j]->Draw();
            }  
        }
    }
}
