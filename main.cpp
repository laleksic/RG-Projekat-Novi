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

MeshPtr MakeScreenQuadMesh() {
    MeshPtr screenQuad = make_shared<Mesh>();
    screenQuad->Positions = {
        vec3(-1,-1,0),
        vec3(1,-1,0),
        vec3(1,1,0),
        vec3(-1,1,0)
    };
    screenQuad->TexCoords = {
        vec2(0,0),
        vec2(1,0),
        vec2(1,1),
        vec2(0,1)
    };
    screenQuad->Elements = {
        0,1,2,
        2,3,0
    };
    screenQuad->UploadToGPU();
    return screenQuad;
}

MeshPtr MakeSpotlightMesh(
    float halfAngle,
    float height,
    int steps
) {
    vec3 tip = vec3(0,0,0);
    //float c = cos(halfAngle);
    //float s = sin(halfAngle);
    // c/s == height/radius
    // radius == (s*height)/c
    //float r = (s*height)/c;
    float r = tan(halfAngle)*height;
    vec3 baseCenter = tip + height * vec3(0,0,-1);
    vector<vec3> baseVertices;
    vector<GLuint> indices;
    float angleStep = radians(360.0f)/steps;
    for (int i=0; i<steps; ++i) {
        vec3 offset;
        offset.x = cos(angleStep*i);
        offset.y = sin(angleStep*i);
        vec3 vertex = baseCenter + offset;
        baseVertices.push_back(vertex);
        indices.push_back(i);
        indices.push_back((i+1)%steps);
        indices.push_back(steps);
    }
    
    MeshPtr cone = make_shared<Mesh>();
    cone->Positions = baseVertices;
    cone->Positions.push_back(tip);
    cone->Elements = indices;
    cone->UploadToGPU();
    return cone;
}

class Framebuffer {
    GLuint FBO;
    vector<GLuint> Textures;
    vector<GLenum> Formats;
    bool MakeDepthBuffer;
    bool SyncWithWindowSize;

    void CheckStatus() {
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
            // default: cerr << "Fbo ok" << endl;
            #undef TMP
        }
    }
    void AttachTextures() {
        int colorAttachCount = Textures.size();
        if (MakeDepthBuffer) {
            colorAttachCount -= 1;
            glNamedFramebufferTexture(FBO, GL_DEPTH_STENCIL_ATTACHMENT, Textures.back(), 0);
        } 

        vector<GLenum> drawBufs;
        for (int buf=0; buf<colorAttachCount; ++buf) {
            glNamedFramebufferTexture(FBO, GL_COLOR_ATTACHMENT0+buf, Textures[buf], 0);
            drawBufs.push_back(GL_COLOR_ATTACHMENT0+buf);
        }
        glNamedFramebufferDrawBuffers(FBO, drawBufs.size(), drawBufs.data());        
    }
    void CreateTextures(ivec2 dims) {
        glCreateTextures(GL_TEXTURE_2D, Textures.size(), &Textures[0]);
        for (int i=0; i<Textures.size(); ++i)
            glTextureStorage2D(Textures[i], 1, Formats[i], dims.x, dims.y);        
    }
public:
    Framebuffer(vector<GLenum> formats, bool makeDepthBuffer, bool syncWithWindowSize, int w=1, int h=1) {
        Formats = formats;
        MakeDepthBuffer = makeDepthBuffer;
        SyncWithWindowSize = syncWithWindowSize;
        if (makeDepthBuffer) {
            Formats.push_back(GL_DEPTH24_STENCIL8);
        }
        Textures.resize(Formats.size());
        glCreateFramebuffers(1, &FBO);
        
        ivec2 dims;
        if (SyncWithWindowSize) {
            dims = TheEngine->GetWindowSize();
        } else {
            dims = ivec2(w,h);
        }
        CreateTextures(dims);
        AttachTextures();
        CheckStatus();
    }
    ~Framebuffer() {
        glDeleteTextures(Textures.size(), &Textures[0]);
        glDeleteFramebuffers(1, &FBO);
    }
    void Update() {
        if (SyncWithWindowSize && TheEngine->WasWindowResized()) {
            glDeleteTextures(Textures.size(), &Textures[0]);
            CreateTextures(TheEngine->GetWindowSize());
            AttachTextures();
            CheckStatus();
        }
    }
    GLuint GetTexture(int i) { return Textures.at(i); }
    void Bind() {
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    }
};

typedef shared_ptr<Framebuffer> FramebufferPtr;

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
    FramebufferPtr GBuffer, RSM;
    ShaderPtr ShadowmapStage;
    ShaderPtr GeometryStage;
    ShaderPtr LightingStage;
    MeshPtr ScreenQuad;
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
    float AttenConst = 0;
    float AttenLin = 0;
    float AttenQuad = 1;
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
        RSM = make_shared<Framebuffer>(
            vector<GLuint>{GL_RGBA32F, GL_RGBA32F, GL_RGBA8},
            true, false, SHADOWMAP_SIZE, SHADOWMAP_SIZE
        );
        GBuffer = make_shared<Framebuffer>(
            vector<GLuint>{GL_RGBA32F, GL_RGBA8, GL_RGBA8, GL_RGBA32F, GL_RGBA8},
            true, true
        );
        for (int buf=0; buf<RSMBufferCount; ++buf){
            vec4 black(0,0,0,1);
            glTextureParameteri(RSM->GetTexture(buf), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER );
            glTextureParameteri(RSM->GetTexture(buf), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER );
            glTextureParameterfv(RSM->GetTexture(buf), GL_TEXTURE_BORDER_COLOR, value_ptr(black));
        }

        ScreenQuad = MakeScreenQuadMesh();

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
        GBuffer->Update();
        RSM->Update();

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
        LightingStage->SetUniform("AttenConst", AttenConst);
        LightingStage->SetUniform("AttenLin", AttenLin);
        LightingStage->SetUniform("AttenQuad", AttenQuad);
        LightingStage->SetUniform("RSMSamplingRadius", (float)RSMSamplingRadius);
        LightingStage->SetUniform("RSMVPLCount", RSMVPLCount);
        LightingStage->SetUniform("RSMReflectionFact", RSMReflectionFact);
        LightingStage->SetUniform("VisualizeIndirectLighting", VisualizeIndirectLighting);
        LightingStage->SetUniform("EnableIndirectLighting", EnableIndirectLighting);
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
        
        RSM->Bind();

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
        
        GBuffer->Bind();

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
            glBindTextureUnit(unit++, GBuffer->GetTexture(buf));
        }
        for (int buf=0; buf<RSMBufferCount; ++buf) {
            glBindTextureUnit(unit++, RSM->GetTexture(buf));
        }
        ScreenQuad->Draw();
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
    drenderer.Flashlight.CutoffAng = radians(25.0f);
    drenderer.Flashlight.Color = vec3(1,1,1);

    camera.Update();
    drenderer.Update(camera); // This has to be called before main loop!!
                              // otherwise fbo incomplete
                              // maybe due to window size.

    ModelPtr sponza = Load<Model>("Data/models/sponza.obj");

    while (TheEngine->Run()) {
        { // Imgui widgets...
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
        ImGui::SliderFloat("Atten Const", &drenderer.AttenConst, 0, 1);
        ImGui::SliderFloat("Atten Linear", &drenderer.AttenLin, 0, 1);
        ImGui::SliderFloat("Atten Quadratic", &drenderer.AttenQuad, 0, 1);
        ImGui::SliderFloat("Sampling Radius", &drenderer.RSMSamplingRadius, 0, 1);
        ImGui::SliderFloat("Reflection Factor", &drenderer.RSMReflectionFact, 0, 1);
        ImGui::SliderInt("VPL Count", &drenderer.RSMVPLCount, 0, 128);
        ImGui::Checkbox("Enable Indirect Light", &drenderer.EnableIndirectLighting);
        ImGui::Checkbox("Visualize Just Indirect Light", &drenderer.VisualizeIndirectLighting);
        }

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
