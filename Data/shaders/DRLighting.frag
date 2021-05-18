#version 450 core

#define PositionBuf 0
#define DiffuseBuf 1
#define SpecularBuf 2
#define NormalBuf 3
#define TranslucencyBuf 4

#define BufferCount 5

#define RSMPositionBuf 0
#define RSMNormalBuf 1
#define RSMFluxBuf 2

#define RSMDepthBuf 3

#define RSMBufferCount 4

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
uniform sampler2D RSM[RSMBufferCount];
uniform int VisualizeBuffer;
uniform int VisualizeRSMBuffer;
uniform bool VisualizeShadowmap;
uniform bool Tonemap;
uniform vec3 CameraPosition;
uniform float Gamma;
uniform float FogDensity;
uniform int RaymarchSteps;
uniform bool RealisticAttenuation;

in VertexData {
    vec2 TexCoords;
} vertexData;

out vec4 Color;

float AttenuateLight(float distanceToLight) {
    float constant, linear, quadratic;
    if (RealisticAttenuation) {
        constant = 0.0f;
        linear = 0.0f;
        quadratic = 1.0f;
    } else {
        constant = 1;
        linear = 0.22f;
        quadratic = 0.20f;
    }

    return 1/(constant + linear*distanceToLight + quadratic*distanceToLight*distanceToLight);
}

vec3 Gamma_ToLinear(vec3 c) {return pow(c,vec3(Gamma));}
vec3 Gamma_FromLinear(vec3 c) {return pow(c,vec3(1/Gamma));}

//https://www.geeks3d.com/20100628/3d-programming-ready-to-use-64-sample-poisson-disc/
const vec2 poissonDisk[64] = vec2[](
vec2(-0.613392, 0.617481),
vec2(0.170019, -0.040254),
vec2(-0.299417, 0.791925),
vec2(0.645680, 0.493210),
vec2(-0.651784, 0.717887),
vec2(0.421003, 0.027070),
vec2(-0.817194, -0.271096),
vec2(-0.705374, -0.668203),
vec2(0.977050, -0.108615),
vec2(0.063326, 0.142369),
vec2(0.203528, 0.214331),
vec2(-0.667531, 0.326090),
vec2(-0.098422, -0.295755),
vec2(-0.885922, 0.215369),
vec2(0.566637, 0.605213),
vec2(0.039766, -0.396100),
vec2(0.751946, 0.453352),
vec2(0.078707, -0.715323),
vec2(-0.075838, -0.529344),
vec2(0.724479, -0.580798),
vec2(0.222999, -0.215125),
vec2(-0.467574, -0.405438),
vec2(-0.248268, -0.814753),
vec2(0.354411, -0.887570),
vec2(0.175817, 0.382366),
vec2(0.487472, -0.063082),
vec2(-0.084078, 0.898312),
vec2(0.488876, -0.783441),
vec2(0.470016, 0.217933),
vec2(-0.696890, -0.549791),
vec2(-0.149693, 0.605762),
vec2(0.034211, 0.979980),
vec2(0.503098, -0.308878),
vec2(-0.016205, -0.872921),
vec2(0.385784, -0.393902),
vec2(-0.146886, -0.859249),
vec2(0.643361, 0.164098),
vec2(0.634388, -0.049471),
vec2(-0.688894, 0.007843),
vec2(0.464034, -0.188818),
vec2(-0.440840, 0.137486),
vec2(0.364483, 0.511704),
vec2(0.034028, 0.325968),
vec2(0.099094, -0.308023),
vec2(0.693960, -0.366253),
vec2(0.678884, -0.204688),
vec2(0.001801, 0.780328),
vec2(0.145177, -0.898984),
vec2(0.062655, -0.611866),
vec2(0.315226, -0.604297),
vec2(-0.780145, 0.486251),
vec2(-0.371868, 0.882138),
vec2(0.200476, 0.494430),
vec2(-0.494552, -0.711051),
vec2(0.612476, 0.705252),
vec2(-0.578845, -0.768792),
vec2(-0.772454, -0.090976),
vec2(0.504440, 0.372295),
vec2(0.155736, 0.065157),
vec2(0.391522, 0.849605),
vec2(-0.620106, -0.328104),
vec2(0.789239, -0.419965),
vec2(-0.545396, 0.538133),
vec2(-0.178564, -0.596057)
);

//http://glampert.com/2014/01-26/visualizing-the-depth-buffer/
float LinearizeDepth(float depth)
{
    float zNear = 0.1;//0.5;    // TODO: Replace by the zNear of your perspective projection
    float zFar  = 250.0;//2000.0; // TODO: Replace by the zFar  of your perspective projection
    return (2.0 * zNear) / (zFar + zNear - depth * (zFar - zNear));
}

float ShadowFactor(vec3 wsPosition){
    float shadowFactor;
    //https://www.gamedev.net/forums/topic/665740-cant-fix-shadow-acne-with-bias/
    vec4 lsPosition = ShadowmapVPMat * vec4(wsPosition,1);
    lsPosition.xyz /= lsPosition.w; // Perspective divide
    float lsFragDepth = (lsPosition.z + 1) / 2;
    vec2 shadowUv = (lsPosition.xy + vec2(1)) / 2;
    float closestDepth = texture(RSM[RSMDepthBuf], shadowUv).r;

    if (closestDepth < lsFragDepth)
        shadowFactor = 0; // in shadow
    else
        shadowFactor = 1; // lit

    return shadowFactor;
}

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

// Raymarch volumetric light
vec3 RaymarchVolumetric(vec3 wsPosition) {
    vec3 wsToCamera = (CameraPosition-wsPosition);
    vec3 rayStep = wsToCamera / RaymarchSteps;;
    vec3 curPos = wsPosition;

    vec3 accum;
    for (int i=0; i<RaymarchSteps; ++i){
        float cutoffFactor = CutoffFactor(normalize(FlashlightPosition-curPos));
        float shadowFactor = ShadowFactor(curPos);        
        float attenuation = AttenuateLight(length(FlashlightPosition-curPos));
        accum += FlashlightColor * cutoffFactor * shadowFactor * attenuation * FogDensity;
        curPos += rayStep;
    }
    return accum / RaymarchSteps;
}

void HandlePointLight(
    in vec3 wsPosition,
    in vec3 wsNormal,
    in vec3 wsToCamera,
    in Light l,
    // Samples from the various maps used in shading
    in vec3 diffuse,
    in vec3 specular,
    in vec3 translucency
){
    vec3 wsLightPosition = l.Position;
    vec3 lightColor = l.Color;
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

void main() {
    Color.rgb = vec3(0);
    Color.a = 1;

    // Color.rgb = texture(RSM[RSMFluxBuf], vertexData.TexCoords).rgb;
    // return;
    // if (VisualizeShadowmap) {
    //     Color.rgb = vec3(LinearizeDepth(texture2D(Shadowmap, vertexData.TexCoords).x));
    //     return;
    // }    
    if (VisualizeBuffer >=0 && VisualizeBuffer <BufferCount) {
        Color.rgb = texture(GBuffer[VisualizeBuffer], vertexData.TexCoords).rgb;
        return;
    }
    if (VisualizeRSMBuffer >=0 && VisualizeRSMBuffer <RSMBufferCount) {
        Color.rgb = texture(RSM[VisualizeRSMBuffer], vertexData.TexCoords).rgb;
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
        HandlePointLight(wsPosition, wsNormal, wsToCamera, Lights[i], diffuse, specular, translucency);
    }

    // // The flashlight 
    // // I'll be repeating some code here which I should refactor into functions..
    {
        vec3 wsToLight = FlashlightPosition - wsPosition;
        vec3 lightColor = FlashlightColor;
        float distanceToLight = length(wsToLight);
        wsToLight = normalize(wsToLight);
    
        float attenuation = AttenuateLight(distanceToLight);
        float cutoffFactor = CutoffFactor(wsToLight);
        float shadowFactor = ShadowFactor(wsPosition+wsNormal*0.05);

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

    {
        vec4 lsPosition = ShadowmapVPMat * vec4(wsPosition,1);
        lsPosition.xyz /= lsPosition.w; // Perspective divide
        float lsFragDepth = (lsPosition.z + 1) / 2;
        vec2 shadowUv = (lsPosition.xy + vec2(1)) / 2;
        const int VPL_COUNT = 64;
        const float samplingRadius = 0.1f;
        for (int i=0; i<VPL_COUNT; ++i) {
            vec2 vplUv = shadowUv + poissonDisk[i] * samplingRadius;
            Light vpl;
            vpl.Position = texture(RSM[RSMPositionBuf], vplUv).rgb;
            vpl.Color = texture(RSM[RSMFluxBuf], vplUv).rgb;
            vec3 vplSurfaceNormal = texture(RSM[RSMNormalBuf], vplUv).rgb;
            float attenuation =
                AttenuateLight(length(vpl.Position-FlashlightPosition) + length(vpl.Position-wsPosition));
            // ---The normals must be facing each other---
            // float align = max(0, -dot(wsNormal, vplSurfaceNormal)); // (why not this??)
            float align = max(0, dot(wsNormal, vplSurfaceNormal));
            // Not all light is reflected
            float refl = 0.5;

            vec3 wsToLight = normalize(vpl.Position-wsPosition);
            // Diffuse
            float lambert = max(0, dot(wsToLight, wsNormal));
            Color.rgb += diffuse * vpl.Color * lambert * attenuation * align;
            float lambertBack = max(0, dot(wsToLight, -wsNormal));
            Color.rgb += diffuse * vpl.Color * lambertBack * attenuation * translucency * align * refl;

            // Specular
            vec3 halfway = normalize(wsToLight + wsToCamera);
            float alignSpec = max(0, dot(halfway, wsNormal));
            float shininess = pow(alignSpec, 8);
            Color.rgb += specular * vpl.Color * shininess * attenuation * align * refl;
        }
    }


    Color.rgb += RaymarchVolumetric(wsPosition+wsNormal*0.05);

    // Gamma correction
    Color.rgb = Gamma_FromLinear( Color.rgb );

    // Tone mapping (Reinhard tone mapping)
    if (Tonemap)
        Color.rgb /= Color.rgb + vec3(1);
}