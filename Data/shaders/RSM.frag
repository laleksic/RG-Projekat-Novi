#version 450 core

in vec2 texCoords;
in vec3 wsPosition;
in vec3 wsNormal;

uniform sampler2D DiffuseMap;
uniform vec3 FlashlightPosition;
uniform vec3 FlashlightDirection;
uniform vec3 FlashlightColor;
uniform float FlashlightCutoffAng;

out vec4 RSMPositionBuf;
out vec4 RSMNormalBuf;
out vec4 RSMFluxBuf;

// Are we in the spotlight cone? 
float CutoffFactor(vec3 wsToLight) {
    float cutoffFactor = max(0, dot(normalize(FlashlightDirection), -wsToLight));
    if (cutoffFactor < cos(FlashlightCutoffAng)){
        cutoffFactor = 0; // no
    } else {
        cutoffFactor =1 ; // yes
    }
    return cutoffFactor;
}

void main() {
    if (texture(DiffuseMap, texCoords).a < 0.5)
        discard;

    RSMPositionBuf.rgb = wsPosition.xyz;
    RSMNormalBuf.rgb = normalize(wsNormal).xyz;
    
    
    float cutoffFactor = CutoffFactor(normalize(FlashlightPosition-wsPosition));
    RSMFluxBuf.rgb = cutoffFactor * texture(DiffuseMap,texCoords).rgb * FlashlightColor;
}