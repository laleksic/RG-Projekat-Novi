struct Light {
    vec3 Position; 
    vec3 Color;
};

uniform mat4 ModelViewProjection;
uniform mat4 Model;
uniform sampler2D DiffuseTexture;
uniform sampler2D SpecularTexture;
uniform Light Lights[32];
uniform vec3 CameraPosition;

#if defined(VERTEX_SHADER)
out
#elif defined(FRAGMENT_SHADER)
in
#endif
VertexData {
    vec3 Color;
    vec2 TexCoords;
    vec3 WorldSpacePosition;
    vec3 WorldSpaceNormal;
} vertexData;

#if defined(VERTEX_SHADER)
    layout (location=0) in vec3 Position;
    layout (location=1) in vec3 Color;
    layout (location=2) in vec2 TexCoords;
    layout (location=3) in vec3 Normal;

    void main() {
        gl_Position.xyz = Position;
        gl_Position.w = 1.0f;
        gl_Position = ModelViewProjection * gl_Position;
        vertexData.Color = Color;
        vertexData.TexCoords = TexCoords;
        vertexData.WorldSpacePosition = vec3(Model * vec4(Position, 1));
        vertexData.WorldSpaceNormal = normalize(vec3(Model * vec4(Normal, 0)));
    }
#elif defined(FRAGMENT_SHADER)
    out vec4 Color;

    float AttenuateLight(float distanceToLight) {
        const float constant = 1.0f;
        const float linear = 0.22f;//0.35f;
        const float quadratic = 0.20f;//0.44f;
        return 1/(constant + linear*distanceToLight + quadratic*distanceToLight*distanceToLight);
    }

    void main() {
        vec4 color = vec4(0,0,0,1);
        vec4 diffuseSample = texture(DiffuseTexture, vertexData.TexCoords);
        vec4 specularSample = texture(SpecularTexture, vertexData.TexCoords);
        if (diffuseSample.a < 0.5) {
            discard;
        }

        vec3 toCamera = CameraPosition - vertexData.WorldSpacePosition;
        
        for (int i=0; i<32; ++i) {
            vec3 toLight = Lights[i].Position - vertexData.WorldSpacePosition;
            float distanceToLight = length(toLight);
            float lambertFactor = max(0,dot(normalize(toLight), vertexData.WorldSpaceNormal));
            float attenuation = AttenuateLight(distanceToLight);
            float diffuseStrength = lambertFactor;

            vec3 halfway = (normalize(toCamera)+normalize(toLight))*0.5;
            float specularStrength = max(0,dot(halfway,vertexData.WorldSpaceNormal));
            specularStrength = pow(specularStrength, 32);

            // diffuseStrength = 0;
            color += attenuation * diffuseStrength * vec4(Lights[i].Color,1) * diffuseSample;
            color += attenuation * specularStrength * vec4(Lights[i].Color,1) * specularSample;
        }
        Color = color;
    }
#endif