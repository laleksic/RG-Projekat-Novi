#version 450 core

in vec2 texCoords;
in vec3 wsPosition;
in vec3 wsNormal;

uniform sampler2D DiffuseMap;

out vec4 RSMPositionBuf;
out vec4 RSMNormalBuf;
out vec4 RSMFluxBuf;

void main() {
    if (texture(DiffuseMap, texCoords).a < 0.5)
        discard;

    RSMPositionBuf.rgb = wsPosition.xyz;
    RSMNormalBuf.rgb = normalize(wsNormal).xyz;
    RSMFluxBuf.rgba = vec4(0,0,0,1); // todo
}