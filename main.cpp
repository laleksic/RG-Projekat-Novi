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
    float Gamma =2.2f;
    vec3 AmbientLight = vec3(1);
    vector<Light> Lights;
    const int MAX_LIGHTS = 100; // Keep in sync with shader!

    DeferredRenderer() {
        glCreateTextures(GL_TEXTURE_2D, BufferCount, &GBuffer[0]);
        glCreateFramebuffers(1, &FBO);

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
        ScreenQuad.Elements = {
            0,1,2,
            2,3,0
        };
        ScreenQuad.UploadToGPU();
        GeometryStage = Load<Shader>("Data/shaders/DRGeometry");
        GeometryStage->SetUniform("DiffuseMap", 0);  
        GeometryStage->SetUniform("SpecularMap", 1);  
        GeometryStage->SetUniform("NormalMap", 2);  
        GeometryStage->SetUniform("BumpMap", 3);           
        GeometryStage->SetUniform("TranslucencyMap", 4);           
        LightingStage = Load<Shader>("Data/shaders/DRLighting");
        for (int buf=0; buf<DepthBuf; ++buf) {
            LightingStage->SetUniform("GBuffer["+to_string(buf)+"]", buf);
        }
    }
    void Update(const Camera& camera) {
        if (TheEngine->WasWindowResized()) {
            glDeleteTextures(BufferCount, &GBuffer[0]);
            glCreateTextures(GL_TEXTURE_2D, BufferCount, &GBuffer[0]);

            ivec2 dims = TheEngine->GetWindowSize();
            glTextureStorage2D(GBuffer[PositionBuf], 1, GL_RGBA32F, dims.x, dims.y);
            glTextureStorage2D(GBuffer[DiffuseBuf], 1, GL_RGB8, dims.x, dims.y);
            glTextureStorage2D(GBuffer[SpecularBuf], 1, GL_RGB8, dims.x, dims.y);
            glTextureStorage2D(GBuffer[NormalBuf], 1, GL_RGBA32F, dims.x, dims.y);
            glTextureStorage2D(GBuffer[TranslucencyBuf], 1, GL_RGB8, dims.x, dims.y);
            glTextureStorage2D(GBuffer[DepthBuf], 1, GL_DEPTH_COMPONENT16, dims.x, dims.y);
        }
        vector<GLenum> drawBufs;
        for (int buf=0; buf<DepthBuf; ++buf) {
            glNamedFramebufferTexture(FBO, GL_COLOR_ATTACHMENT0+buf, GBuffer[buf], 0);
            drawBufs.push_back(GL_COLOR_ATTACHMENT0+buf);
        }
        glNamedFramebufferTexture(FBO, GL_DEPTH_ATTACHMENT, GBuffer[DepthBuf], 0);
        glNamedFramebufferDrawBuffers(FBO, drawBufs.size(), drawBufs.data());

        GLenum fboStatus = glCheckNamedFramebufferStatus(FBO, GL_FRAMEBUFFER);
        switch (fboStatus) {
            #define TMP(v) case v: cerr<< #v << endl; abort(); break;
            TMP(GL_FRAMEBUFFER_UNDEFINED)
            TMP(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
            TMP(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
            TMP(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER)
            TMP(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER)
            TMP(GL_FRAMEBUFFER_UNSUPPORTED)
            TMP(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)
            TMP(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS)
            default: cerr << "Fbo ok" << endl;
            #undef TMP
        }

        ivec2 windowSize = TheEngine->GetWindowSize();
        float aspectRatio = (float)windowSize.x / windowSize.y;
        mat4 projectionMat = perspective(radians(60.0f), aspectRatio, 0.1f, 250.0f);  
        VPMat = projectionMat * camera.GetViewMatrix();
        GeometryStage->SetUniform("MVPMat", VPMat);
        GeometryStage->SetUniform("CameraPosition", camera.GetPosition());

        GeometryStage->SetUniform("ParallaxDepth", ParallaxDepth);
        GeometryStage->SetUniform("Gamma", Gamma);

        LightingStage->SetUniform("AmbientLight", AmbientLight);
        LightingStage->SetUniform("LightCount", std::min((int)Lights.size(), MAX_LIGHTS));
        for (int i=0; i<std::min((int)Lights.size(),MAX_LIGHTS); ++i) {
            LightingStage->SetUniform("Lights["+to_string(i)+"].Position", Lights[i].Position);
            LightingStage->SetUniform("Lights["+to_string(i)+"].Color", Lights[i].Color);
        }
        LightingStage->SetUniform("CameraPosition", camera.GetPosition());
        LightingStage->SetUniform("Gamma", Gamma);
    }
    ~DeferredRenderer() {
        glDeleteTextures(BufferCount, &GBuffer[0]);
        glDeleteFramebuffers(1, &FBO);
    }
    void SetModelMatrix(mat4 model) {
        GeometryStage->SetUniform("NormalMat", mat3(transpose(inverse(mat3(model)))));
        GeometryStage->SetUniform("ModelMat", model);
        GeometryStage->SetUniform("MVPMat", VPMat * model);
    }

    void Draw(ModelPtr model) {
        for (int i=0; i<model->Meshes.size(); ++i) {
            SetMaterial(model->Materials[i]);
            model->Meshes[i]->Draw();
        }
    }
    void BeginGeometryStage() {
        SetModelMatrix(mat4(1.0f));
        
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);

        ivec2 windowSize = TheEngine->GetWindowSize();
        glViewport(0,0, windowSize.x, windowSize.y);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        GeometryStage->Use();
    }
    void EndGeometryStage() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    void DoLightingStage() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        ivec2 windowSize = TheEngine->GetWindowSize();

        glViewport(0,0, windowSize.x, windowSize.y);
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

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

void RandomizeLights(DeferredRenderer& rend, int lightCount){
    rend.Lights.clear();
    for (int i=0; i<lightCount; ++i) {
        Light l;
        l.Position = vec3(
                RandomFloat(-7.5, 7.5),
                RandomFloat(0, 7.5),
                RandomFloat(-15, 5)
        );
        l.Color = vec3(RandomFloat(), RandomFloat(), RandomFloat());
        rend.Lights.push_back(l);
    }
}

int main(int argc, char** argv) {
    TheEngine = make_shared<Engine>();
    DeferredRenderer drenderer;
    drenderer.AmbientLight = vec3(0.05);

    FPSCamera camera;
    camera.SetPosition(vec3(0.0f, 2.0f, 2.0f));  
    RandomizeLights(drenderer, 16);

    camera.Update();
    drenderer.Update(camera); // This has to be called before main loop!!
                              // otherwise fbo incomplete
                              // maybe due to window size.

    ModelPtr sponza = Load<Model>("Data/models/sponza.obj");

    while (TheEngine->Run()) {
        ImGui::DragFloat("Parallax depth",&drenderer.ParallaxDepth,
            0.01f, 0, 0.2f, "%f", 1.0f); 
        #define TMP(v) if (ImGui::Button(#v)) {\
            drenderer.VisualizeBuffer(v);\
        }
        TMP(DeferredRenderer::PositionBuf);
        TMP(DeferredRenderer::DiffuseBuf);
        TMP(DeferredRenderer::SpecularBuf);
        TMP(DeferredRenderer::NormalBuf);
        TMP(DeferredRenderer::TranslucencyBuf);
        #undef TMP
        if (ImGui::Button("Final render")) {
            drenderer.VisualizeBuffer(-1);
        }
        static int lightCount = 16;
        ImGui::SliderInt("Light count", &lightCount, 0, drenderer.MAX_LIGHTS);
        if (ImGui::Button("Randomize lights")) {
            RandomizeLights(drenderer, lightCount);
        }
        ImGui::ColorEdit3("Ambient light", value_ptr(drenderer.AmbientLight));
        
        camera.Update();
        drenderer.Update(camera);

        drenderer.BeginGeometryStage();
            drenderer.SetModelMatrix(scale(vec3(0.01f)));
            drenderer.Draw(sponza);
        drenderer.EndGeometryStage();
        
        drenderer.DoLightingStage();
    }
}
