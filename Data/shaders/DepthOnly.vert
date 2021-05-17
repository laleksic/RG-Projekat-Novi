#version 450 core

uniform mat4 MVPMat;

layout (location=0) in vec3 Position;
layout (location=2) in vec2 TexCoords;

out vec2 texCoords;

void main() {
    gl_Position = MVPMat * vec4(Position, 1);
    texCoords = TexCoords;
}