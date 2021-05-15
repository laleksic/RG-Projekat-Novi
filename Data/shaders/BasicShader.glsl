struct Light {
    vec3 Position; 
    vec3 Color;
};

uniform mat4 MVPMat;
uniform mat4 ModelMat;
uniform sampler2D DiffuseTexture;
uniform sampler2D SpecularTexture;
uniform sampler2D NormalTexture;
uniform sampler2D BumpTexture;
uniform Light Lights[32];
uniform vec3 CameraPosition;
uniform vec3 AmbientLight;
uniform bool Translucent;
uniform float ParallaxDepth;

#if defined(VERTEX_SHADER)
out
#elif defined(FRAGMENT_SHADER)
in
#endif
VertexData {
    vec3 Color;
    vec2 TexCoords;
    vec3 WSPosition;
    //mat3 TBNMat;
    //vec3 TSToLight[32];
    mat3 InvTBNMat;
    vec3 TSToCamera;
} vertexData;

#if defined(VERTEX_SHADER)
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
        vertexData.Color = Color;
        vertexData.TexCoords = TexCoords;
        vertexData.WSPosition = (ModelMat * vec4(Position, 1)).xyz;

        // Normal matrix! learnopengl.com/Lighting/Basic-lighting
        mat3 normalMatrix = mat3(transpose(inverse(ModelMat)));
        
        vec3 wsNormal = normalize(normalMatrix * normalize(Normal));
        vec3 wsTangent = normalize(normalMatrix * normalize(Tangent));
        vec3 wsBitangent = normalize(normalMatrix * normalize(Bitangent));
        mat3 invTBNMat = transpose(mat3(wsTangent, wsBitangent, wsNormal));

        vertexData.TSToCamera = invTBNMat * (CameraPosition - vertexData.WSPosition);
        // for (int i=0; i<32; ++i) {
            // vertexData.TSToLight[i] = invTBNMat * (Lights[i].Position - vertexData.WSPosition);
        // }
        vertexData.InvTBNMat = invTBNMat;

    }
#elif defined(FRAGMENT_SHADER)
    out vec4 Color;

    float AttenuateLight(float distanceToLight) {
        const float constant = 1.0f;
        const float linear = 0.22f;//0.35f;
        const float quadratic = 0.20f;//0.44f;
        return 1/(constant + linear*distanceToLight + quadratic*distanceToLight*distanceToLight);
    }

    float ParallaxMappingSelfShadowing(
        in vec3 tsToLight,
        in vec2 st
    ) {
        // This fixes things for "reasons"
        tsToLight.y *= -1;

        // Sort of reverse steep parallax mapping
        const int LAYER_COUNT = 8;
        float bias = 0.01;
        float currLayerDepth = texture(BumpTexture, st).r * ParallaxDepth - bias;
        float depthStep = -currLayerDepth / LAYER_COUNT;
        vec2 stStep = (tsToLight.xy * currLayerDepth) / LAYER_COUNT;

        while (currLayerDepth > 0) {
            currLayerDepth += depthStep;
            st += stStep;           
            
            // check if under surface
            if (currLayerDepth > texture(BumpTexture, st).r * ParallaxDepth) {
                return 0;
            } 
        }
        return 1;
    }

    void ReliefParallaxMapping(
        in vec3 tsToCamera,
        inout vec2 st
    ) {
        // This fixes things for "reasons"
        tsToCamera.y *= -1;

        float mipLevel = (textureQueryLod(BumpTexture, st).y);
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
            if (currLayerDepth > ParallaxDepth * texture(BumpTexture, st).r)
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
            if (currLayerDepth > ParallaxDepth * texture(BumpTexture, st).r) {
                currLayerDepth-=depthStep;
                st-=stStep;
            } else {
                currLayerDepth+=depthStep;
                st+=stStep;
            }
        }
    }

    void main() {
        vec4 color = vec4(0,0,0,1);
        vec3 tsToCamera = normalize(vertexData.TSToCamera);
        vec2 texCoords = vertexData.TexCoords;
        // texCoords = fract(texCoords);
        ReliefParallaxMapping(tsToCamera, texCoords);
        // if (texCoords.s < 0 || texCoords.s > 1 ||
        //     texCoords.t < 0 || texCoords.t > 1 ) {
        //     discard;
        // }

        vec4 diffuseSample = texture(DiffuseTexture, texCoords);
        vec4 specularSample = texture(SpecularTexture, texCoords);
        vec4 normalSample = texture(NormalTexture, texCoords);
        if (diffuseSample.a < 0.5) {
            discard;
        }

        vec3 tsNormal;
        tsNormal = normalSample.rgb;
        tsNormal = 2*tsNormal - vec3(1);
        tsNormal = normalize(tsNormal);

        mat3 invTBNMat = vertexData.InvTBNMat;
        for (int i=0; i<32; ++i) {
            vec3 wsPosition = vertexData.WSPosition;
            vec3 wsLightPosition = Lights[i].Position;
            vec3 tsToLight = invTBNMat * (wsLightPosition - wsPosition);

            float distanceToLight = length(tsToLight);
            tsToLight = normalize(tsToLight);
            float lambertFactor = max(0,dot(tsToLight, tsNormal));
            if (Translucent) {
                float lambertFactorBack = max(0,dot(tsToLight, -tsNormal));
                lambertFactor = max(lambertFactor, lambertFactorBack);
            }
            float attenuation = AttenuateLight(distanceToLight);
            float diffuseStrength = lambertFactor;

            vec3 halfway = normalize(tsToCamera+tsToLight);
            float specularStrength = max(0,dot(halfway,tsNormal));
            specularStrength = pow(specularStrength, 48);

            // diffuseStrength = 0;
            color += attenuation * diffuseStrength * vec4(Lights[i].Color,1) * diffuseSample;
            color += attenuation * specularStrength * vec4(Lights[i].Color,1) * specularSample;
            
            // Too slow (do in deferred?)
            //color *= ParallaxMappingSelfShadowing(tsToLight, texCoords);
        }
        color += vec4(AmbientLight,1) * diffuseSample;
        Color = color;
    }
#endif