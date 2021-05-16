#version 450 core

#define PositionBuf 0
#define DiffuseBuf 1
#define SpecularBuf 2
#define NormalBuf 3
#define TranslucencyBuf 4

#define BufferCount 5

uniform sampler2D GBuffer[BufferCount];
uniform int VisualizeBuffer;

in VertexData {
    vec2 TexCoords;
} vertexData;

out vec4 Color;

void main() {
    Color.a = 1;
    if (VisualizeBuffer >=0 && VisualizeBuffer <BufferCount) {
        Color.rgb = texture(GBuffer[VisualizeBuffer], vertexData.TexCoords).rgb;
        return;
    }
    Color.rgb = vec3(0);
}