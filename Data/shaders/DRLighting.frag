#version 450 core

#define PositionBuf 0
#define DiffuseBuf 1
#define SpecularBuf 2
#define NormalBuf 3
#define TranslucencyBuf 4

#define BufferCount 5
#define MAX_LIGHTS 100

struct Light {
    vec3 Position;
    vec3 Color;
};

uniform Light Lights[MAX_LIGHTS];
uniform int LightCount;
uniform vec3 AmbientLight;
uniform sampler2D GBuffer[BufferCount];
uniform int VisualizeBuffer;
uniform vec3 CameraPosition;
uniform float Gamma;

in VertexData {
    vec2 TexCoords;
} vertexData;

out vec4 Color;

float AttenuateLight(float distanceToLight) {
    const float constant = 1.0f;
    const float linear = 0.22f;//0.35f;
    const float quadratic = 0.20f;//0.44f;
    return 1/(constant + linear*distanceToLight + quadratic*distanceToLight*distanceToLight);
}

vec3 Gamma_ToLinear(vec3 c) {return pow(c,vec3(Gamma));}
vec3 Gamma_FromLinear(vec3 c) {return pow(c,vec3(1/Gamma));}

void main() {
    Color.rgb = vec3(0);
    Color.a = 1;
    if (VisualizeBuffer >=0 && VisualizeBuffer <BufferCount) {
        Color.rgb = texture(GBuffer[VisualizeBuffer], vertexData.TexCoords).rgb;
        return;
    }

    vec3 wsPosition = texture(GBuffer[PositionBuf], vertexData.TexCoords).xyz;
    vec3 wsNormal = texture(GBuffer[NormalBuf], vertexData.TexCoords).xyz;
    vec3 specular = texture(GBuffer[SpecularBuf], vertexData.TexCoords).rgb;
    vec3 diffuse = texture(GBuffer[DiffuseBuf], vertexData.TexCoords).rgb;
    vec3 translucency = texture(GBuffer[TranslucencyBuf], vertexData.TexCoords).rgb;

    vec3 wsToCamera = CameraPosition - wsPosition;

    // Accumulate the various lights
    // (A global ambient light)
    Color.rgb += AmbientLight.rgb * diffuse;

    for (int i=0; i<LightCount; ++i) {
        vec3 wsLightPosition = Lights[i].Position;
        vec3 lightColor = Lights[i].Color;
        vec3 wsToLight = (wsLightPosition - wsPosition);
        float distanceToLight = length(wsToLight);
        wsToLight = normalize(wsToLight);

        float attenuation = AttenuateLight(distanceToLight);

        // Diffuse
        float lambert = max(0, dot(wsToLight, wsNormal));
        Color.rgb += diffuse * lightColor * lambert * attenuation;
        float lambertBack = max(0, dot(wsToLight, -wsNormal));
        Color.rgb += diffuse * lightColor * lambertBack * attenuation * translucency;

        // Specular
        vec3 halfway = normalize(wsToLight + wsToCamera);
        float align = max(0, dot(halfway, wsNormal));
        float shininess = pow(align, 32);
        Color.rgb += specular * lightColor * shininess * attenuation;
    }

    // Gamma correction
    Color.rgb = Gamma_FromLinear( Color.rgb );
}