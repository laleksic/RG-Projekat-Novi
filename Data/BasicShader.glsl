struct Light {
    vec3 Position; 
    vec3 Color;
};

uniform mat4 ModelViewProjection;
uniform mat4 Model;
uniform sampler2D DiffuseTexture;
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

    void main() {
        vec4 color = vec4(0,0,0,1);
        vec4 diffuseSample = texture(DiffuseTexture, vertexData.TexCoords);
        if (diffuseSample.a < 0.5) {
            discard;
        }
        for (int i=0; i<16; ++i) {
            vec3 toLight = Lights[i].Position - vertexData.WorldSpacePosition;
            float distanceToLightSqr = length(toLight)*length(toLight);
            float lambertFactor = max(0,dot(normalize(toLight), normalize(vertexData.WorldSpaceNormal)));
            float arbitraryFactor = 8;
            float attenuation = 1/distanceToLightSqr * arbitraryFactor;
            float diffuseStrength = attenuation * lambertFactor;
            color += (diffuseStrength * vec4(Lights[i].Color,1) * diffuseSample);
        }
        Color = color;

        //vec3 toCamera = CameraPosition - vertexData.WorldSpacePosition;
        //float lambertFactor = max(0,dot(normalize(toCamera), normalize(vertexData.WorldSpaceNormal)));
        //Color = vec4(lambertFactor.xxx, 1);
        //Color = vec4((vertexData.WorldSpaceNormal),1);
    }
#endif