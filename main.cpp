#define STB_IMAGE_IMPLEMENTATION
#include "main.hpp"

class Light {
public:
    vec3 Position;
    vec3 Color;
};

int main(int argc, char** argv) {
    TheEngine = make_shared<Engine>();

    // Load resources
    // ------
    ModelPtr Sponza = Load<Model>("Data/models/sponza.obj");
    ModelPtr Cube = Load<Model>("Data/models/cube.obj");
    ShaderPtr BasicShader = Load<Shader>("Data/shaders/BasicShader.glsl");
    ShaderPtr LightCubeShader = Load<Shader>("Data/shaders/LightCube.glsl");
    
    // -- Setup scene
    vector<Light> Lights[2];
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
            Lights[j].push_back(light);
        }
    }

    // Setup shaders --
    BasicShader->SetUniform("DiffuseTexture", 0);  
    BasicShader->SetUniform("SpecularTexture", 1);  
    BasicShader->SetUniform("NormalTexture", 2);  
    BasicShader->SetUniform("BumpTexture", 3);      

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
        BasicShader->SetUniform("MVPMat", mvpMat);
        BasicShader->SetUniform("ModelMat", modelMat);
        BasicShader->SetUniform("CameraPosition", camera.GetPosition());

        static float parallaxDepth = 0.04f;
        ImGui::DragFloat("Parallax depth",&parallaxDepth,
            0.01f, 0, 0.2f, "%f", 1.0f);     
        BasicShader->SetUniform("ParallaxDepth", parallaxDepth);

        for (int i=0; i<Lights[0].size(); ++i) {
            vec3 lightPosition = lerp(Lights[0][i].Position, Lights[1][i].Position, smoothstep(0.0f,1.0f,lightLerp));
            vec3 lightColor = lerp(Lights[0][i].Color, Lights[1][i].Color, smoothstep(0.0f,1.0f,lightLerp));
            BasicShader->SetUniform("Lights["+to_string(i)+"].Position", lightPosition);
            BasicShader->SetUniform("Lights["+to_string(i)+"].Color", lightColor);
        }
        BasicShader->SetUniform("AmbientLight", vec3(0.075,0.075,0.125));
        lightLerp = (sin(glfwGetTime())+1.0)/2.0;

        //===
        glViewport(0,0, windowSize.x, windowSize.y);
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        // Render standard scene
        // ----------------------
        BasicShader->Use( );
        for (int i=0; i<Sponza->Meshes.size(); ++i) {
            if (Sponza->Materials[i].Translucent) {
                glDisable(GL_CULL_FACE);
                BasicShader->SetUniform("Translucent", true);
            } else {
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
                BasicShader->SetUniform("Translucent", false);
            }
            Sponza->Materials[i].DiffuseMap->Bind(0);
            Sponza->Materials[i].SpecularMap->Bind(1);
            Sponza->Materials[i].NormalMap->Bind(2);
            Sponza->Materials[i].BumpMap->Bind(3);
            Sponza->Meshes[i]->Draw();
        }

        // Render lights as cubes
        // ----------------------
        LightCubeShader->Use();
        for (int i=0; i<Lights[0].size(); ++i) {
            vec3 lightPosition = lerp(Lights[0][i].Position, Lights[1][i].Position, smoothstep(0.0f,1.0f,lightLerp));
            vec3 lightColor = lerp(Lights[0][i].Color, Lights[1][i].Color, smoothstep(0.0f,1.0f,lightLerp));
            modelMat = translate(lightPosition) * scale(vec3(0.125f));
            mvpMat = vpMat * modelMat;
            LightCubeShader->SetUniform("MVPMat", mvpMat);
            LightCubeShader->SetUniform("ModelMat", modelMat);
            LightCubeShader->SetUniform("LightColor", lightColor);
            for (int j=0; j<Cube->Meshes.size(); ++j) {
                Cube->Meshes[j]->Draw();
            }  
        }
    }
}
