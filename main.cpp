#define STB_IMAGE_IMPLEMENTATION
#include "main.hpp"

struct Light {
    vec3 Position;
    vec3 Color;
};

class DeferredRenderer {
public:
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
private:
    GLuint GBuffer[BufferCount];
    GLuint FBO;
    ShaderPtr GeometryStage;
    ShaderPtr LightingStage;
    Mesh ScreenQuad;
    mat4 VPMat;

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
public:
    float ParallaxDepth =0.04f;
    vec3 AmbientLight = vec3(1);
    vector<Light> Lights;
    const int MAX_LIGHTS = 32; // Keep in sync with shader!

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

        LightingStage->SetUniform("AmbientLight", AmbientLight);
        LightingStage->SetUniform("LightCount", std::max((int)Lights.size(), MAX_LIGHTS));
        for (int i=0; i<Lights.size(); ++i) {
            LightingStage->SetUniform("Lights["+to_string(i)+"].Position", Lights[i].Position);
            LightingStage->SetUniform("Lights["+to_string(i)+"].Color", Lights[i].Color);
        }
    }
    ~DeferredRenderer() {
        glDeleteTextures(BufferCount, &GBuffer[0]);
        glDeleteFramebuffers(1, &FBO);
    }
    void SetModelMatrix(mat4 model) {
        GeometryStage->SetUniform("NormalMat", mat3(transpose(inverse(model))));
        GeometryStage->SetUniform("MVPMat", VPMat * model);
    }

    void Draw(ModelPtr model) {
        for (int i=0; i<model->Meshes.size(); ++i) {
            SetMaterial(model->Materials[i]);
            model->Meshes[i]->Draw();
        }
    }
    void BeginGeometryStage() {
        ivec2 windowSize = TheEngine->GetWindowSize();
        glViewport(0,0, windowSize.x, windowSize.y);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

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
    void VisualizeBuffer(int buf) {
        LightingStage->SetUniform("VisualizeBuffer", buf);
    }
};

int main(int argc, char** argv) {
    TheEngine = make_shared<Engine>();
    DeferredRenderer drenderer;

    ModelPtr sponza = Load<Model>("Data/models/sponza.obj");
    
    FPSCamera camera;
    camera.SetPosition(vec3(0.0f, 2.0f, 2.0f));  

    while (TheEngine->Run()) {
        ImGui::DragFloat("Parallax depth",&drenderer.ParallaxDepth,
            0.01f, 0, 0.2f, "%f", 1.0f);             
        
        camera.Update();
        drenderer.Update(camera);

        drenderer.BeginGeometryStage();
            drenderer.SetModelMatrix(scale(vec3(0.01f)));
            drenderer.Draw(sponza);
        drenderer.EndGeometryStage();
        drenderer.DoLightingStage();
    }
}
