#version 450 core

in vec2 texCoords;

uniform sampler2D DiffuseMap;

void main() {
    /*
    if (texture(DiffuseMap, texCoords).a < 0.5)
        discard;
    */
}