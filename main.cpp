#define STB_IMAGE_IMPLEMENTATION
#include "main.hpp"

class Light {
public:
    vec3 Position;
    vec3 Color;
};

class DeferredRenderer {
    enum Buffer {
        PositionBuf,
        DiffuseBuf,
        SpecularBuf,
        NormalBuf,
        TangentBuf,
        BitangentBuf,
        BumpBuf,
        TranslucencyBuf,

        DepthBuf,

        BufferCount
    };
    GLuint GBuffer[BufferCount];
    GLuint FBO;
    ShaderPtr GeometryStage;
    ShaderPtr LightingStage;
    Mesh ScreenQuad;
    mat4 VPMat;
public:
    float ParallaxDepth =0.04f;

    void SetAmbientLight(vec3 ambientLight) {
        LightingStage->SetUniform("AmbientLight", ambientLight);
    }
    void SetLightCount(int count) {
        LightingStage->SetUniform("LightCount", count);
    }
    void SetLight(int i, vec3 pos, vec3 color) {
        LightingStage->SetUniform("Lights["+to_string(i)+"].Position", pos);
        LightingStage->SetUniform("Lights["+to_string(i)+"].Color", color);
    }

    DeferredRenderer() {
        glCreateTextures(GL_TEXTURE_2D, BufferCount, &GBuffer[0]);
        glCreateFramebuffers(1, &FBO);
        for (int buf=0; buf<DepthBuf; ++buf)
            glNamedFramebufferTexture(FBO, GL_COLOR_ATTACHMENT0+buf, GBuffer[buf], 0);
        glNamedFramebufferTexture(FBO, GL_DEPTH_ATTACHMENT, GBuffer[DepthBuf], 0);
        ScreenQuad.Positions = {
            vec3(-1,-1,0),
            vec3(1,-1,0),
            vec3(1,1,0),
            vec3(-1,1,0)
        };
        ScreenQuad.TexCoords = {
            vec2(0,0),
            vec2(1,0),
            vec2(1,1),
            vec2(0,1)
        };
        ScreenQuad.UploadToGPU();
        GeometryStage = Load<Shader>("Data/shaders/DRGeometry.glsl");
        GeometryStage->SetUniform("DiffuseMap", 0);  
        GeometryStage->SetUniform("SpecularMap", 1);  
        GeometryStage->SetUniform("NormalMap", 2);  
        GeometryStage->SetUniform("BumpMap", 3);           
        GeometryStage->SetUniform("TranslucencyMap", 4);           
        LightingStage = Load<Shader>("Data/shaders/DRLighting.glsl");
        for (int buf=0; buf<DepthBuf; ++buf) {
            LightingStage->SetUniform("GBuffer["+to_string(buf)+"]", buf);
        }
    }
    void Update(Camera& camera) {
        if (TheEngine->WasWindowResized()) {
            ivec2 dims = TheEngine->GetWindowSize();
            glTextureStorage2D(GBuffer[PositionBuf], 1, GL_RGB32F, dims.x, dims.y);
            glTextureStorage2D(GBuffer[DiffuseBuf], 1, GL_RGB8, dims.x, dims.y);
            glTextureStorage2D(GBuffer[SpecularBuf], 1, GL_RGB8, dims.x, dims.y);
            glTextureStorage2D(GBuffer[NormalBuf], 1, GL_RGB32F, dims.x, dims.y);
            glTextureStorage2D(GBuffer[TangentBuf], 1, GL_RGB32F, dims.x, dims.y);
            glTextureStorage2D(GBuffer[BitangentBuf], 1, GL_RGB32F, dims.x, dims.y);
            glTextureStorage2D(GBuffer[BumpBuf], 1, GL_RGB8, dims.x, dims.y);
            glTextureStorage2D(GBuffer[TranslucencyBuf], 1, GL_RGB8, dims.x, dims.y);
            glTextureStorage2D(GBuffer[DepthBuf], 1, GL_DEPTH_COMPONENT16, dims.x, dims.y);
        }

        ivec2 windowSize = TheEngine->GetWindowSize();
        float aspectRatio = (float)windowSize.x / windowSize.y;
        mat4 projectionMat = perspective(radians(60.0f), aspectRatio, 0.1f, 250.0f);  
        VPMat = projectionMat * camera.GetViewMatrix();
        GeometryStage->SetUniform("MVPMat", VPMat);

        GeometryStage->SetUniform("ParallaxDepth", ParallaxDepth);
    }
    ~DeferredRenderer() {
        glDeleteTextures(BufferCount, &GBuffer[0]);
        glDeleteFramebuffers(1, &FBO);
    }
    void SetModelMatrix(mat4 model) {
        GeometryStage->SetUniform("NormalMat", mat3(transpose(inverse(model))));
        GeometryStage->SetUniform("MVPMat", VPMat * model);
    }
    void SetMaterial(Material mat) {
        mat.DiffuseMap->Bind(0);
        mat.SpecularMap->Bind(1);
        mat.NormalMap->Bind(2);
        mat.BumpMap->Bind(3);
        mat.TranslucencyMap->Bind(4);
        if (mat.DiffuseMap->ShouldAlphaClip())
            glDisable(GL_CULL_FACE);
        else
            glEnable(GL_CULL_FACE);
    }
    void Draw(ModelPtr model) {
        for (int i=0; i<model->Meshes.size(); ++i) {
            SetMaterial(model->Materials[i]);
            model->Meshes[i]->Draw();
        }
    }
    void BeginGeometryStage() {
        GeometryStage->Use();
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    }
    void EndGeometryStage() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    void DoLightingStage() {
        LightingStage->Use();
        for (int buf=0; buf<DepthBuf; ++buf) {
            glBindTextureUnit(buf, GBuffer[buf]);
        }
        ScreenQuad.Draw();
    }
};

int main(int argc, char** argv) {
    TheEngine = make_shared<Engine>();

    // Load resources
    // ------
    ModelPtr sponza = Load<Model>("Data/models/sponza.obj");
    ModelPtr cube = Load<Model>("Data/models/cube.obj");
    ShaderPtr basicShader = Load<Shader>("Data/shaders/BasicShader.glsl");
    ShaderPtr lightCubeShader = Load<Shader>("Data/shaders/LightCube.glsl");
    
    // -- Setup scene
    vector<Light> lights[2];
    float lightLerp = 0.0f;
    FPSCamera camera;

    camera.SetPosition(vec3(0.0f, 2.0f, 2.0f));  

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
            lights[j].push_back(light);
        }
    }

    // Setup shaders --
    basicShader->SetUniform("DiffuseTexture", 0);  
    basicShader->SetUniform("SpecularTexture", 1);  
    basicShader->SetUniform("NormalTexture", 2);  
    basicShader->SetUniform("BumpTexture", 3);      

    while (TheEngine->Run()) {
        // Update non-gpu stuff
        // ---------
        camera.Update();

        // Calculate stuff we'll need
        // ----
        ivec2 windowSize = TheEngine->GetWindowSize();
        float aspectRatio = (float)windowSize.x / windowSize.y;
        
        mat4 modelMat = scale(vec3(0.01f));
        mat4 projectionMat = perspective(radians(60.0f), aspectRatio, 0.1f, 250.0f);  
        mat4 vpMat = projectionMat * camera.GetViewMatrix();
        mat4 mvpMat = vpMat * modelMat;

        // Update per-frame uniforms
        // ----------------
        basicShader->SetUniform("MVPMat", mvpMat);
        basicShader->SetUniform("ModelMat", modelMat);
        basicShader->SetUniform("CameraPosition", camera.GetPosition());

        static float parallaxDepth = 0.04f;
        ImGui::DragFloat("Parallax depth",&parallaxDepth,
            0.01f, 0, 0.2f, "%f", 1.0f);     
        basicShader->SetUniform("ParallaxDepth", parallaxDepth);

        for (int i=0; i<lights[0].size(); ++i) {
            vec3 lightPosition = lerp(lights[0][i].Position, lights[1][i].Position, smoothstep(0.0f,1.0f,lightLerp));
            vec3 lightColor = lerp(lights[0][i].Color, lights[1][i].Color, smoothstep(0.0f,1.0f,lightLerp));
            basicShader->SetUniform("Lights["+to_string(i)+"].Position", lightPosition);
            basicShader->SetUniform("Lights["+to_string(i)+"].Color", lightColor);
        }
        basicShader->SetUniform("AmbientLight", vec3(0.075,0.075,0.125));
        lightLerp = (sin(glfwGetTime())+1.0)/2.0;

        //===
        glViewport(0,0, windowSize.x, windowSize.y);
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        // Render standard scene
        // ----------------------
        basicShader->Use( );
        for (int i=0; i<sponza->Meshes.size(); ++i) {
            if (sponza->Materials[i].Translucent) {
                glDisable(GL_CULL_FACE);
                basicShader->SetUniform("Translucent", true);
            } else {
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
                basicShader->SetUniform("Translucent", false);
            }
            sponza->Materials[i].DiffuseMap->Bind(0);
            sponza->Materials[i].SpecularMap->Bind(1);
            sponza->Materials[i].NormalMap->Bind(2);
            sponza->Materials[i].BumpMap->Bind(3);
            sponza->Meshes[i]->Draw();
        }

        // Render lights as cubes
        // ----------------------
        lightCubeShader->Use();
        for (int i=0; i<lights[0].size(); ++i) {
            vec3 lightPosition = lerp(lights[0][i].Position, lights[1][i].Position, smoothstep(0.0f,1.0f,lightLerp));
            vec3 lightColor = lerp(lights[0][i].Color, lights[1][i].Color, smoothstep(0.0f,1.0f,lightLerp));
            modelMat = translate(lightPosition) * scale(vec3(0.125f));
            mvpMat = vpMat * modelMat;
            lightCubeShader->SetUniform("MVPMat", mvpMat);
            lightCubeShader->SetUniform("ModelMat", modelMat);
            lightCubeShader->SetUniform("LightColor", lightColor);
            for (int j=0; j<cube->Meshes.size(); ++j) {
                cube->Meshes[j]->Draw();
            }  
        }
    }
}
