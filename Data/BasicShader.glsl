struct Light {
    vec3 Position; 
    vec3 Color;
};

uniform mat4 ModelViewProjection;
uniform sampler2D DiffuseTexture;
uniform Light Lights[32];

#if defined(VERTEX_SHADER)
    layout (location=0) in vec3 Position;
    layout (location=1) in vec3 Color;
    layout (location=2) in vec2 TexCoords;
    out VertexData {
        vec3 Color;
        vec2 TexCoords;
        vec3 WorldSpacePosition;
    } vertexData;

    void main() {
        gl_Position.xyz = Position;
        gl_Position.w = 1.0f;
        gl_Position = ModelViewProjection * gl_Position;
        vertexData.Color = Color;
        vertexData.TexCoords = TexCoords;
        vertexData.WorldSpacePosition = Position;
    }
#elif defined(FRAGMENT_SHADER)
    in VertexData {
        vec3 Color;
        vec2 TexCoords;
        vec3 WorldSpacePosition;
    } vertexData;

    out vec4 Color;

    void main() {
        vec4 color = vec4(0,0,0,1);
        vec4 diffuseSample = texture(DiffuseTexture, vertexData.TexCoords);
        if (diffuseSample.a < 0.5) {
            discard;
        }
        for (int i=0; i<32; ++i) {
            vec3 toLight = Lights[i].Position - vertexData.WorldSpacePosition;
            float distanceToLightSqr = length(toLight)*length(toLight);
            //float lambertFactor = ;
            float arbitraryFactor = 8;
            float attenuation = 1/distanceToLightSqr * arbitraryFactor;
            float diffuseStrength = attenuation;
            color += (diffuseStrength * vec4(Lights[i].Color,1) * diffuseSample);
        }
        Color = color;
    }
#endif