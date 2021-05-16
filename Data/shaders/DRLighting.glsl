#define PositionBuf 0
#define DiffuseBuf 1
#define SpecularBuf 2
#define NormalBuf 3
#define TranslucencyBuf 4

#define BufferCount 5

uniform sampler2D GBuffer[BufferCount];
uniform int VisualizeBuffer;

#if defined(VERTEX_SHADER)
out
#elif defined(FRAGMENT_SHADER)
in
#endif
VertexData {
    vec2 TexCoords;
} vertexData;

#if defined(VERTEX_SHADER)
    layout (location=0) in vec3 Position;
    layout (location=2) in vec2 TexCoords;

    void main() {
        gl_Position.xyz = Position;
        gl_Position.w = 1.0f;
        vertexData.TexCoords = TexCoords;
    }
#elif defined(FRAGMENT_SHADER)
    out vec4 Color;

    void main() {
        Color.a = 1;
        if (VisualizeBuffer >=0 && VisualizeBuffer <BufferCount) {
            Color.rgb = texture(GBuffer[VisualizeBuffer], vertexData.TexCoords).rgb;
            return;
        }
        Color.rgb = vec3(0);
    }
#endif