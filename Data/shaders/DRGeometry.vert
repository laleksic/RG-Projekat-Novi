#version 450 core

uniform vec3 CameraPosition;
uniform mat4 MVPMat;
uniform mat4 ModelMat;
uniform mat3 NormalMat;
uniform sampler2D DiffuseMap;
uniform sampler2D SpecularMap;
uniform sampler2D NormalMap;
uniform sampler2D BumpMap;
uniform sampler2D TranslucencyMap;
uniform float ParallaxDepth;

out VertexData {
    vec2 TexCoords;
    vec3 WSPosition;
    vec3 WSNormal;
    vec3 TSToCamera;
    mat3 Tangent2World;
} vertexData;

layout (location=0) in vec3 Position;
layout (location=1) in vec3 Color;
layout (location=2) in vec2 TexCoords;
layout (location=3) in vec3 Normal;
layout (location=4) in vec3 Tangent;
layout (location=5) in vec3 Bitangent;

void main() {
    gl_Position.xyz = Position;
    gl_Position.w = 1.0f;
    gl_Position = MVPMat * gl_Position;
    vertexData.WSPosition = (ModelMat * vec4(Position,1)).xyz;
    vertexData.TexCoords = TexCoords;
    vertexData.WSNormal = normalize( NormalMat * Normal );
    vec3 wsTangent = normalize( NormalMat * Tangent );
    vec3 wsBitangent = normalize( NormalMat * Bitangent );
    vertexData.Tangent2World = mat3(
        wsTangent, 
        wsBitangent, 
        vertexData.WSNormal
        );
    vertexData.TSToCamera = (
        inverse(vertexData.Tangent2World) * (CameraPosition - vertexData.WSPosition)
    );
}