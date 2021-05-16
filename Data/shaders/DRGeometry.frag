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
uniform float Gamma;

in VertexData {
    vec2 TexCoords;
    vec3 WSPosition;
    vec3 WSNormal;
    vec3 TSToCamera;
    mat3 Tangent2World;
} vertexData;

//G-Buffer
out vec3 PositionBuf;
out vec3 DiffuseBuf;
out vec3 SpecularBuf;
out vec3 NormalBuf;
out vec3 TranslucencyBuf;

float ParallaxMappingQuality(vec3 tsToCamera, vec2 st) {
    float mipLevel = (textureQueryLod(BumpMap, st).y);
    float align = 1-max(0, dot(tsToCamera, vec3(0,0,1)));
    if (mipLevel < 0.1) {
        return 1;
    } else {
        return align * 1/(mipLevel);
    }
}

void ReliefParallaxMapping(
    in vec3 tsToCamera,
    inout vec2 st
) {
    // This fixes things for "reasons"
    tsToCamera.y *= -1;

    float quality = ParallaxMappingQuality(tsToCamera, st);

    // Steep parallax mapping
    float minLayers = 4;
    float maxLayers = 32;
    float layerCount = mix(minLayers, maxLayers, quality);
    float depthStep = ParallaxDepth / layerCount;
    vec2 stStep = -(tsToCamera.xy * ParallaxDepth) / layerCount;

    float currLayerDepth = 0;
    while (currLayerDepth < ParallaxDepth) {
        // check if under surface
        if (currLayerDepth > ParallaxDepth * texture(BumpMap, st).r)
            break;

        currLayerDepth += depthStep;
        st += stStep;
    }

    // Relief parallax mapping
    float minSteps = 2;
    float maxSteps = 16;
    float reliefSteps = mix(minSteps, maxSteps, quality);
    for (int i=0; i<int(reliefSteps); ++i) {
        depthStep /= 2;
        stStep /= 2;
        // check if under surface
        if (currLayerDepth > ParallaxDepth * texture(BumpMap, st).r) {
            currLayerDepth-=depthStep;
            st-=stStep;
        } else {
            currLayerDepth+=depthStep;
            st+=stStep;
        }
    }
}

vec3 Normal2RGB(vec3 n){ return (n+1)/2; }
vec3 RGB2Normal(vec3 c){ return c*2-1;}
vec3 Gamma_ToLinear(vec3 c) {return pow(c,vec3(Gamma));}
vec3 Gamma_FromLinear(vec3 c) {return pow(c,vec3(1/Gamma));}

void main() {
    vec2 texCoords = vertexData.TexCoords;
    if (texture(DiffuseMap, texCoords).a < 0.5) {
        discard;
    }
    vec3 tsToCamera = normalize( vertexData.TSToCamera );
    ReliefParallaxMapping(tsToCamera, texCoords);
    PositionBuf = vertexData.WSPosition;
    DiffuseBuf = Gamma_ToLinear( texture(DiffuseMap, texCoords).rgb );
    SpecularBuf = texture(SpecularMap, texCoords).rgb;
    NormalBuf = vertexData.Tangent2World * RGB2Normal(texture(NormalMap, texCoords).rgb);
    TranslucencyBuf = texture(TranslucencyMap, texCoords).rgb;
}