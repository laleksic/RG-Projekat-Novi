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
