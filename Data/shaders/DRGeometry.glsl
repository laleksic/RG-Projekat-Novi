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

#if defined(VERTEX_SHADER)
out
#elif defined(FRAGMENT_SHADER)
in
#endif
VertexData {
    vec2 TexCoords;
    vec3 WSPosition;
    vec3 WSNormal;
    vec3 TSToCamera;
    mat3 Tangent2World;
} vertexData;

#if defined(VERTEX_SHADER)
#line 1
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
        vertexData.TSToCamera = inverse(vertexData.Tangent2World) * (CameraPosition - vertexData.WSPosition);
        vertexData.TSToCamera = normalize(vertexData.TSToCamera);
    }
#elif defined(FRAGMENT_SHADER)
    //G-Buffer
    out vec3 PositionBuf;
    out vec3 DiffuseBuf;
    out vec3 SpecularBuf;
    out vec3 NormalBuf;
    out vec3 TranslucencyBuf;

    void ReliefParallaxMapping(
        in vec3 tsToCamera,
        inout vec2 st
    ) {
        // This fixes things for "reasons"
        tsToCamera.y *= -1;

        float mipLevel = (textureQueryLod(BumpMap, st).y);
        float align = 1-max(0, dot(tsToCamera, vec3(0,0,1)));
        float quality;
        if (mipLevel < 0.1) {
            quality = 1;
        } else {
            quality = align * 1/(mipLevel);
        }

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

    void main() {
        vec2 texCoords = vertexData.TexCoords;
        // ReliefParallaxMapping(vertexData.TSToCamera, texCoords);
        PositionBuf = vertexData.WSPosition;
        DiffuseBuf = texture(DiffuseMap, texCoords).rgb;
        SpecularBuf = texture(SpecularMap, texCoords).rgb;
        NormalBuf = Normal2RGB(vertexData.Tangent2World * RGB2Normal(texture(NormalMap, texCoords).rgb));
        TranslucencyBuf = texture(TranslucencyMap, texCoords).rgb;
    }
#endif