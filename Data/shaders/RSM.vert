#version 450 core

uniform mat4 MVPMat;
uniform mat4 ModelMat;
uniform mat3 NormalMat;

layout (location=0) in vec3 Position;
layout (location=2) in vec2 TexCoords;
layout (location=3) in vec3 Normal;

out vec2 texCoords;
out vec3 wsPosition;
out vec3 wsNormal;

void main() {
    gl_Position = MVPMat * vec4(Position, 1);
    texCoords = TexCoords;
    wsPosition = vec3(ModelMat * vec4(Position,1));
    wsNormal = NormalMat * Normal;
}