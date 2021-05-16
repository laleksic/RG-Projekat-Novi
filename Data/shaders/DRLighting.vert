#version 450 core

#define PositionBuf 0
#define DiffuseBuf 1
#define SpecularBuf 2
#define NormalBuf 3
#define TranslucencyBuf 4

#define BufferCount 5

uniform sampler2D GBuffer[BufferCount];
uniform int VisualizeBuffer;

out VertexData {
    vec2 TexCoords;
} vertexData;

layout (location=0) in vec3 Position;
layout (location=2) in vec2 TexCoords;

void main() {
    gl_Position.xyz = Position;
    gl_Position.w = 1.0f;
    vertexData.TexCoords = TexCoords;
}