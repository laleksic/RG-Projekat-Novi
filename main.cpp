// Sources
// RSM: https://ericpolman.com/2016/03/17/reflective-shadow-maps/
// Vol.Light: http://www.alexandre-pestana.com/volumetric-lights/

#define STB_IMAGE_IMPLEMENTATION
#include "main.hpp"

struct Light {
    vec3 Position;
    vec3 Color;
};

// Inherits from Camera so I don't repeat the direction/view matrix stuff..
//- -- --
struct Spotlight: public Camera{ 
    vec3 Color;
    float CutoffAng;
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

    enum RSMBuffer {
        RSMPositionBuf,
        RSMNormalBuf,
        RSMFluxBuf,

        RSMDepthBuf,

        RSMBufferCount
    };
private:
    GLuint GBuffer[BufferCount];
    GLuint RSM[RSMBufferCount];
    GLuint Shadowmap;
    GLuint ShadowmapFBO;
    GLuint GeometryFBO;
    ShaderPtr ShadowmapStage;
    ShaderPtr GeometryStage;
    ShaderPtr LightingStage;
    Mesh ScreenQuad;
    mat4 ShadowmapVPMat;
    mat4 GeometryVPMat;

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
    float Gamma =2.2;
    float FogDensity = 0.01f;
    int RaymarchSteps=48;
    bool RealisticAttenuation=true;
    bool Tonemap =true;
    bool VisualizeShadowmap = false;
    vec3 AmbientLight = vec3(1);
    vector<Light> Lights;
    const int MAX_LIGHTS = 100; // Keep in sync with shader!
    const int SHADOWMAP_SIZE = 512;
    Spotlight Flashlight;
    float RSMSamplingRadius=0.1;
    int RSMVPLCount=64;
    float RSMReflectionFact=0.5;
    bool VisualizeIndirectLighting = false;
    bool EnableIndirectLighting = true;    

    DeferredRenderer() {
        glCreateTextures(GL_TEXTURE_2D, RSMBufferCount, &RSM[0]);
        glTextureStorage2D(RSM[RSMDepthBuf], 1, GL_DEPTH_COMPONENT16, SHADOWMAP_SIZE, SHADOWMAP_SIZE);
        glTextureStorage2D(RSM[RSMPositionBuf], 1, GL_RGBA32F, SHADOWMAP_SIZE, SHADOWMAP_SIZE);
        glTextureStorage2D(RSM[RSMNormalBuf], 1, GL_RGBA32F, SHADOWMAP_SIZE, SHADOWMAP_SIZE);
        glTextureStorage2D(RSM[RSMFluxBuf], 1, GL_RGBA8, SHADOWMAP_SIZE, SHADOWMAP_SIZE);
        for (int buf=0; buf<RSMBufferCount; ++buf){
            vec4 black(0,0,0,1);
            glTextureParameteri(RSM[buf], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER );
            glTextureParameteri(RSM[buf], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER );
            glTextureParameterfv(RSM[buf], GL_TEXTURE_BORDER_COLOR, value_ptr(black));
        }

        glCreateFramebuffers(1, &ShadowmapFBO);
        vector<GLenum> drawBufs;
        for (int buf=0; buf<RSMDepthBuf; ++buf) {
            glNamedFramebufferTexture(ShadowmapFBO, GL_COLOR_ATTACHMENT0+buf, RSM[buf], 0);
            drawBufs.push_back(GL_COLOR_ATTACHMENT0+buf);
        }
        glNamedFramebufferDrawBuffers(ShadowmapFBO, drawBufs.size(), drawBufs.data());        
        glNamedFramebufferTexture(ShadowmapFBO, GL_DEPTH_ATTACHMENT, RSM[RSMDepthBuf], 0);

        glCreateTextures(GL_TEXTURE_2D, BufferCount, &GBuffer[0]);
        glCreateFramebuffers(1, &GeometryFBO);

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
        ShadowmapStage = Load<Shader>("Data/shaders/RSM");
        ShadowmapStage->SetUniform("DiffuseMap", 0);  
        GeometryStage = Load<Shader>("Data/shaders/DRGeometry");
        GeometryStage->SetUniform("DiffuseMap", 0);  
        GeometryStage->SetUniform("SpecularMap", 1);  
        GeometryStage->SetUniform("NormalMap", 2);  
        GeometryStage->SetUniform("BumpMap", 3);           
        GeometryStage->SetUniform("TranslucencyMap", 4);           
        LightingStage = Load<Shader>("Data/shaders/DRLighting");
        int unit = 0;
        for (int buf=0;buf<DepthBuf; ++buf) {
            LightingStage->SetUniform("GBuffer["+to_string(buf)+"]", unit++);
        }
        for (int buf=0;buf<RSMBufferCount; ++buf) {
            LightingStage->SetUniform("RSM["+to_string(buf)+"]", unit++);
        }

        VisualizeRSMBuffer(-1);
        VisualizeBuffer(-1); // go straight to final render.
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
            glNamedFramebufferTexture(GeometryFBO, GL_COLOR_ATTACHMENT0+buf, GBuffer[buf], 0);
            drawBufs.push_back(GL_COLOR_ATTACHMENT0+buf);
        }
        glNamedFramebufferTexture(GeometryFBO, GL_DEPTH_ATTACHMENT, GBuffer[DepthBuf], 0);
        glNamedFramebufferDrawBuffers(GeometryFBO, drawBufs.size(), drawBufs.data());

        GLenum fboStatus = glCheckNamedFramebufferStatus(GeometryFBO, GL_FRAMEBUFFER);
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
            // default: cerr << "Fbo ok" << endl;
            #undef TMP
        }
        fboStatus = glCheckNamedFramebufferStatus(ShadowmapFBO, GL_FRAMEBUFFER);
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
            // default: cerr << "Fbo ok" << endl;
            #undef TMP
        }        

        ivec2 windowSize = TheEngine->GetWindowSize();
        float aspectRatio = (float)windowSize.x / windowSize.y;
        mat4 projectionMat = perspective(radians(60.0f), aspectRatio, 0.1f, 250.0f);  
        GeometryVPMat = projectionMat * camera.GetViewMatrix();
        GeometryStage->SetUniform("MVPMat", GeometryVPMat);
        GeometryStage->SetUniform("ModelMat", mat4(1));
        GeometryStage->SetUniform("NormalMat", mat3(1));
        GeometryStage->SetUniform("CameraPosition", camera.GetPosition());
        ShadowmapVPMat = perspective(2*Flashlight.CutoffAng, 1.0f, 0.1f, 250.0f) * Flashlight.GetViewMatrix();
        ShadowmapStage->SetUniform("MVPMat", ShadowmapVPMat);
        ShadowmapStage->SetUniform("ModelMat", mat4(1));
        ShadowmapStage->SetUniform("NormalMat", mat3(1));
        ShadowmapStage->SetUniform("FlashlightPosition", Flashlight.GetPosition());
        ShadowmapStage->SetUniform("FlashlightDirection", Flashlight.GetDirection());
        ShadowmapStage->SetUniform("FlashlightColor", Flashlight.Color);
        ShadowmapStage->SetUniform("FlashlightCutoffAng", Flashlight.CutoffAng);        

        GeometryStage->SetUniform("ParallaxDepth", ParallaxDepth);
        GeometryStage->SetUniform("Gamma", Gamma);

        LightingStage->SetUniform("AmbientLight", AmbientLight);
        LightingStage->SetUniform("LightCount", std::min((int)Lights.size(), MAX_LIGHTS));
        for (int i=0; i<std::min((int)Lights.size(),MAX_LIGHTS); ++i) {
            LightingStage->SetUniform("Lights["+to_string(i)+"].Position", Lights[i].Position);
            LightingStage->SetUniform("Lights["+to_string(i)+"].Color", Lights[i].Color);
        }
        LightingStage->SetUniform("FlashlightPosition", Flashlight.GetPosition());
        LightingStage->SetUniform("FlashlightDirection", Flashlight.GetDirection());
        LightingStage->SetUniform("FlashlightColor", Flashlight.Color);
        LightingStage->SetUniform("FlashlightCutoffAng", Flashlight.CutoffAng);
        LightingStage->SetUniform("CameraPosition", camera.GetPosition());
        LightingStage->SetUniform("Gamma", Gamma);
        LightingStage->SetUniform("FogDensity", FogDensity);
        LightingStage->SetUniform("RaymarchSteps", RaymarchSteps);
        LightingStage->SetUniform("Tonemap", Tonemap);
        LightingStage->SetUniform("VisualizeShadowmap", VisualizeShadowmap);
        LightingStage->SetUniform("ShadowmapVPMat", ShadowmapVPMat);
        LightingStage->SetUniform("RealisticAttenuation", RealisticAttenuation);
        LightingStage->SetUniform("RSMSamplingRadius", (float)RSMSamplingRadius);
        LightingStage->SetUniform("RSMVPLCount", RSMVPLCount);
        LightingStage->SetUniform("RSMReflectionFact", RSMReflectionFact);
        LightingStage->SetUniform("VisualizeIndirectLighting", VisualizeIndirectLighting);
        LightingStage->SetUniform("EnableIndirectLighting", EnableIndirectLighting);
    }
    ~DeferredRenderer() {
        glDeleteTextures(BufferCount, &GBuffer[0]);
        glDeleteTextures(1, &Shadowmap);
        glDeleteFramebuffers(1, &GeometryFBO);
        glDeleteFramebuffers(1, &ShadowmapFBO);
    }
    void SetModelMatrix(mat4 model) {
        mat3 normalMat = mat3(transpose(inverse(mat3(model))));
        GeometryStage->SetUniform("NormalMat", normalMat);
        GeometryStage->SetUniform("ModelMat", model);
        GeometryStage->SetUniform("MVPMat", GeometryVPMat * model);
        ShadowmapStage->SetUniform("MVPMat", ShadowmapVPMat * model);
        ShadowmapStage->SetUniform("NormalMat", normalMat);
        ShadowmapStage->SetUniform("ModelMat", model);
    }

    void Draw(ModelPtr model) {
        for (int i=0; i<model->Meshes.size(); ++i) {
            SetMaterial(model->Materials[i]);
            model->Meshes[i]->Draw();
        }
    }
    void BeginShadowmapStage() {
        SetModelMatrix(mat4(1.0f));
        
        glBindFramebuffer(GL_FRAMEBUFFER, ShadowmapFBO);

        glViewport(0,0, SHADOWMAP_SIZE, SHADOWMAP_SIZE);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        ShadowmapStage->Use();
    }
    void EndShadowmapStage() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    void BeginGeometryStage() {
        SetModelMatrix(mat4(1.0f));
        
        glBindFramebuffer(GL_FRAMEBUFFER, GeometryFBO);

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

        int unit=0;
        for (int buf=0; buf<DepthBuf; ++buf) {
            glBindTextureUnit(unit++, GBuffer[buf]);
        }
        for (int buf=0; buf<RSMBufferCount; ++buf) {
            glBindTextureUnit(unit++, RSM[buf]);
        }
        ScreenQuad.Draw();
    }
    void VisualizeBuffer(int buf) {
        LightingStage->SetUniform("VisualizeBuffer", buf);
    }
    void VisualizeRSMBuffer(int buf) {
        LightingStage->SetUniform("VisualizeRSMBuffer", buf);
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

void AnimateLights(DeferredRenderer& rend) {
    for (Light& l: rend.Lights) {
        l.Position.y += sin(glfwGetTime()+l.Color.r)*0.06;
        l.Position.x += sin(glfwGetTime()+l.Color.g)*0.06;
        l.Position.z += sin(glfwGetTime()+l.Color.b)*0.06;
    }
}

void SyncFlashlightToCamera(DeferredRenderer& rend, Camera& cam) {
    rend.Flashlight.SetPosition( cam.GetPosition() );
    rend.Flashlight.SetPitch( cam.GetPitch());
    rend.Flashlight.SetYaw( cam.GetYaw());
}

void RotateFlashlight(DeferredRenderer& rend) {
    rend.Flashlight.SetYaw(rend.Flashlight.GetYaw()+0.3f);
}

int main(int argc, char** argv) {
    TheEngine = make_shared<Engine>();
    DeferredRenderer drenderer;
    drenderer.AmbientLight = vec3(0.05);

    FPSCamera camera;
    camera.SetPosition(vec3(0.0f, 2.0f, 2.0f));  
    RandomizeLights(drenderer, 16);
    drenderer.Flashlight.CutoffAng = radians(45.0f);
    drenderer.Flashlight.Color = vec3(1,1,1);

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
            drenderer.VisualizeRSMBuffer(-1);\
        }
        TMP(DeferredRenderer::PositionBuf);
        TMP(DeferredRenderer::DiffuseBuf);
        TMP(DeferredRenderer::SpecularBuf);
        TMP(DeferredRenderer::NormalBuf);
        TMP(DeferredRenderer::TranslucencyBuf);
        #undef TMP
        #define TMP(v) if(ImGui::Button(#v)) {\
            drenderer.VisualizeRSMBuffer(v);\
            drenderer.VisualizeBuffer(-1);\
        }
        TMP(DeferredRenderer::RSMPositionBuf);
        TMP(DeferredRenderer::RSMNormalBuf);
        TMP(DeferredRenderer::RSMFluxBuf);
        #undef TMP
        if (ImGui::Button("Final render")) {
            drenderer.VisualizeBuffer(-1);
            drenderer.VisualizeRSMBuffer(-1);
        }
        static int lightCount = 16;
        ImGui::SliderInt("Light count", &lightCount, 0, drenderer.MAX_LIGHTS);
        if (ImGui::Button("Randomize lights")) {
            RandomizeLights(drenderer, lightCount);
        }
        ImGui::ColorEdit3("Ambient light", value_ptr(drenderer.AmbientLight));
        static bool animateLights = true;
        ImGui::Checkbox("Animate lights", &animateLights);
        if (animateLights)
            AnimateLights(drenderer);
        static bool syncFlashlightToCamera = true;
        ImGui::Checkbox("Sync flashlight to camera", &syncFlashlightToCamera);
        static bool rotateFlashlight = false;
        ImGui::Checkbox("Rotate flashlight", &rotateFlashlight);
        if (rotateFlashlight)
            RotateFlashlight(drenderer);
        if (syncFlashlightToCamera)
            SyncFlashlightToCamera(drenderer, camera);
        ImGui::SliderAngle("Flashlight cutoff angle", &drenderer.Flashlight.CutoffAng,
            0.0f, 60.0f);
        ImGui::ColorEdit3("Flashlight color", value_ptr(drenderer.Flashlight.Color));
        ImGui::Checkbox("Visualize shadowmap", &drenderer.VisualizeShadowmap);
        ImGui::SliderFloat("Gamma", &drenderer.Gamma, 1.0f, 2.2f);
        ImGui::Checkbox("Reinhard Tonemapping", &drenderer.Tonemap);
        ImGui::SliderFloat("Fog Density", &drenderer.FogDensity, 0, 2);
        ImGui::SliderInt("Raymarch Steps", &drenderer.RaymarchSteps, 16, 96);
        ImGui::Checkbox("Realistic Light Attenuation", &drenderer.RealisticAttenuation);
        ImGui::SliderFloat("Sampling Radius", &drenderer.RSMSamplingRadius, 0, 1);
        ImGui::SliderFloat("Reflection Factor", &drenderer.RSMReflectionFact, 0, 1);
        ImGui::SliderInt("VPL Count", &drenderer.RSMVPLCount, 0, 64);
        ImGui::Checkbox("Enable Indirect Light", &drenderer.EnableIndirectLighting);
        ImGui::Checkbox("Visualize Just Indirect Light", &drenderer.VisualizeIndirectLighting);

        camera.Update();
        drenderer.Update(camera);

        drenderer.BeginShadowmapStage();
            drenderer.SetModelMatrix(scale(vec3(0.01f)));
            drenderer.Draw(sponza);
        drenderer.EndShadowmapStage();

        drenderer.BeginGeometryStage();
            drenderer.SetModelMatrix(scale(vec3(0.01f)));
            drenderer.Draw(sponza);
        drenderer.EndGeometryStage();
        
        drenderer.DoLightingStage();
    }
}
