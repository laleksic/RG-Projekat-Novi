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
uniform vec3 FlashlightPosition;
uniform vec3 FlashlightDirection;
uniform vec3 FlashlightColor;
uniform float FlashlightCutoffAng;
uniform mat4 ShadowmapVPMat;
uniform sampler2D GBuffer[BufferCount];
uniform sampler2D Shadowmap;
uniform int VisualizeBuffer;
uniform bool VisualizeShadowmap;
uniform bool Tonemap;
uniform vec3 CameraPosition;
uniform float Gamma;

in VertexData {
    vec2 TexCoords;
} vertexData;

out vec4 Color;

float AttenuateLight(float distanceToLight) {
    const float constant = 0.0f;//1.0f;
    const float linear = 0.0f;//0.22f;//0.35f;
    const float quadratic = 1.0f;//0.20f;//0.44f;
    return 1/(constant + linear*distanceToLight + quadratic*distanceToLight*distanceToLight);
}

vec3 Gamma_ToLinear(vec3 c) {return pow(c,vec3(Gamma));}
vec3 Gamma_FromLinear(vec3 c) {return pow(c,vec3(1/Gamma));}

//http://glampert.com/2014/01-26/visualizing-the-depth-buffer/
float LinearizeDepth(float depth)
{
    float zNear = 0.1;//0.5;    // TODO: Replace by the zNear of your perspective projection
    float zFar  = 250.0;//2000.0; // TODO: Replace by the zFar  of your perspective projection
    return (2.0 * zNear) / (zFar + zNear - depth * (zFar - zNear));
}


void main() {
    Color.rgb = vec3(0);
    Color.a = 1;
    // if (VisualizeShadowmap) {
    //     Color.rgb = vec3(LinearizeDepth(texture2D(Shadowmap, vertexData.TexCoords).x));
    //     return;
    // }    
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

    // Point lights
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

    // // The flashlight 
    // // I'll be repeating some code here which I should refactor into functions..
    {
        vec3 wsToLight = FlashlightPosition - wsPosition;
        vec3 lightColor = FlashlightColor;
        float distanceToLight = length(wsToLight);
        wsToLight = normalize(wsToLight);
    
        float attenuation = AttenuateLight(distanceToLight);
        float cutoffFactor = max(0, dot(normalize(FlashlightDirection), -wsToLight));
        if (cutoffFactor < cos(FlashlightCutoffAng)){
            cutoffFactor = 0;
        }
        float shadowFactor;
        //https://www.gamedev.net/forums/topic/665740-cant-fix-shadow-acne-with-bias/
        vec3 normalOffset = wsNormal * 0.05f;
        vec4 lsPosition = ShadowmapVPMat * vec4(wsPosition+normalOffset,1);
        lsPosition.xyz /= lsPosition.w; // Perspective divide
        float lsFragDepth = (lsPosition.z + 1) / 2;
        vec2 shadowUv = (lsPosition.xy + vec2(1)) / 2;
        float closestDepth = texture(Shadowmap, shadowUv).r;

        // debug
        // ---
        if (VisualizeShadowmap) {
            Color.rgb = vec3(LinearizeDepth(closestDepth));
            return;
        }
        // Color.rgb = lsPosition.xyz;
        // ---
        
        
        if (closestDepth < lsFragDepth)
            shadowFactor = 0; // in shadow
        else
            shadowFactor = 1; // lit

        // Diffuse
        float lambert = max(0, dot(wsToLight, wsNormal));
        Color.rgb += diffuse * lightColor * lambert * attenuation * cutoffFactor * shadowFactor;
        float lambertBack = max(0, dot(wsToLight, -wsNormal));
        Color.rgb += diffuse * lightColor * lambertBack * attenuation * translucency * cutoffFactor * shadowFactor;

        // Specular
        vec3 halfway = normalize(wsToLight + wsToCamera);
        float align = max(0, dot(halfway, wsNormal));
        float shininess = pow(align, 32);
        Color.rgb += specular * lightColor * shininess * attenuation * cutoffFactor * shadowFactor;        
    }

    // Gamma correction
    Color.rgb = Gamma_FromLinear( Color.rgb );

    // Tone mapping (Reinhard tone mapping)
    if (Tonemap)
        Color.rgb /= Color.rgb + vec3(1);
}