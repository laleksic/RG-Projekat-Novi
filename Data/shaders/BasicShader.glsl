struct Light {
    vec3 Position; 
    vec3 Color;
};

uniform mat4 ModelViewProjection;
uniform mat4 Model;
uniform sampler2D DiffuseTexture;
uniform sampler2D SpecularTexture;
uniform sampler2D NormalTexture;
uniform sampler2D BumpTexture;
uniform Light Lights[32];
uniform vec3 CameraPosition;
uniform vec3 AmbientLight;
uniform bool UseNormalMaps;
uniform bool VisualizeNormals;
uniform bool UseSpecular;
uniform bool HighlightZeroNormals;
uniform bool NormalizeAfterConvertingToWorldSpace;
uniform bool Translucent;
uniform bool VisualizeBumpMap;
uniform bool NoLighting;
uniform float ParallaxStrength;

#if defined(VERTEX_SHADER)
out
#elif defined(FRAGMENT_SHADER)
in
#endif
VertexData {
    vec3 Color;
    vec2 TexCoords;
    vec3 WorldSpacePosition;
    vec3 WorldSpaceNormal;
    mat3 TangentBitangentNormalMatrix;
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
        gl_Position = ModelViewProjection * gl_Position;
        vertexData.Color = Color;
        vertexData.TexCoords = TexCoords;
        vertexData.WorldSpacePosition = (Model * vec4(Position, 1)).xyz;

        // Normal matrix! learnopengl.com/Lighting/Basic-lighting
        mat3 normalMatrix = mat3(transpose(inverse(Model)));
        
        vec3 worldSpaceNormal = normalize(normalMatrix * normalize(Normal));
        vec3 worldSpaceTangent = normalize(normalMatrix * normalize(Tangent));
        vec3 worldSpaceBitangent = normalize(normalMatrix * normalize(Bitangent));
        vertexData.WorldSpaceNormal = worldSpaceNormal;
        vertexData.TangentBitangentNormalMatrix = mat3(worldSpaceTangent, worldSpaceBitangent, worldSpaceNormal);
    }
#elif defined(FRAGMENT_SHADER)
    out vec4 Color;

    float AttenuateLight(float distanceToLight) {
        const float constant = 1.0f;
        const float linear = 0.22f;//0.35f;
        const float quadratic = 0.20f;//0.44f;
        return 1/(constant + linear*distanceToLight + quadratic*distanceToLight*distanceToLight);
    }

    void main() {
        vec4 color = vec4(0,0,0,1);
        vec4 bumpSample = texture(BumpTexture, vertexData.TexCoords);
        float depth = bumpSample.r;
        vec3 tangentSpaceCameraPosition = vertexData.TangentBitangentNormalMatrix * CameraPosition;
        vec3 tangentSpacePosition = vertexData.TangentBitangentNormalMatrix * vertexData.WorldSpacePosition;
        vec3 tangentSpaceToCamera = normalize(tangentSpaceCameraPosition - tangentSpacePosition);
        vec2 texCoordsOffset = (tangentSpaceToCamera * depth).xy;
        vec2 texCoords = vertexData.TexCoords + ParallaxStrength * texCoordsOffset;
      
        // vec3 position = vertexData.WorldSpacePosition + normalize(toCamera) * depth;
        vec3 position = vertexData.WorldSpacePosition;
        vec3 toCamera = CameraPosition - position;

        vec4 diffuseSample = texture(DiffuseTexture, texCoords);
        vec4 specularSample = texture(SpecularTexture, texCoords);
        vec4 normalSample = texture(NormalTexture, texCoords);
        if (diffuseSample.a < 0.5) {
            discard;
        }

        vec3 normal;
        if (UseNormalMaps) {
            //Trouble is somewhere here?
            normal = normalSample.rgb;
            normal = 2*normal - vec3(1);
            normal = normalize(normal);

            normal = vertexData.TangentBitangentNormalMatrix * normal;
            if (NormalizeAfterConvertingToWorldSpace)          
                normal = normalize(normal);
        } else {
            normal = normalize(vertexData.WorldSpaceNormal);
        }

        

        
        for (int i=0; i<32; ++i) {
            vec3 toLight = Lights[i].Position - position;
            float distanceToLight = length(toLight);
            float lambertFactor = max(0,dot(normalize(toLight), normal));
            if (Translucent) {
                float lambertFactorBack = max(0,dot(normalize(toLight), -normal));
                lambertFactor = max(lambertFactor, lambertFactorBack);
            }
            float attenuation = AttenuateLight(distanceToLight);
            float diffuseStrength = lambertFactor;

            vec3 halfway = normalize(normalize(toCamera)+normalize(toLight));
            float specularStrength = max(0,dot(halfway,normal));
            specularStrength = pow(specularStrength, 48);

            // diffuseStrength = 0;
            color += attenuation * diffuseStrength * vec4(Lights[i].Color,1) * diffuseSample;
            if (UseSpecular)
                color += attenuation * specularStrength * vec4(Lights[i].Color,1) * specularSample;
        }
        color += vec4(AmbientLight,1) * diffuseSample;
        Color = color;
        
        if (VisualizeNormals)
            Color = vec4((normal+vec3(1))/2, 1);
        else if (VisualizeBumpMap)
            Color = bumpSample;
        else if (NoLighting)
            Color = diffuseSample;

        if (length(normal) < 0.1 && HighlightZeroNormals)
            Color = vec4(1,0,0,1);

        //Color = normalSample;
    }
#endif