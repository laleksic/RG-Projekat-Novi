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
uniform float AttenConst;
uniform float AttenLin;
uniform float AttenQuad;
uniform float RSMSamplingRadius;
uniform float RSMReflectionFact;
uniform int RSMVPLCount;
uniform bool VisualizeIndirectLighting;
uniform bool EnableIndirectLighting;

in VertexData {
    vec2 TexCoords;
} vertexData;

out vec4 Color;

float AttenuateLight(float distanceToLight) {
    return 1/(AttenConst + AttenLin*distanceToLight + AttenQuad*distanceToLight*distanceToLight);
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

// Code used to generate
/*
import numpy as np
import math
print("const vec2 importanceSample[] = vec2[](")
for i in range(64):
  angle = np.random.uniform(0,2*math.pi)
  length = np.random.normal()
  x=math.cos(angle)
  y=math.sin(angle)
  x*=length
  y*=length
  print(f"vec2({x},{y})",end='')
  if i != 63:
    print(",")
  else:
    print()
print(");")
*/
const vec2 importanceSample[] = vec2[](
vec2(0.07317615435725791,1.2749927780376944),
vec2(0.5072097789667972,-0.21682922601734708),
vec2(-0.26483949626562975,-0.009026176173089873),
vec2(0.11421449298251311,0.9981497459404837),
vec2(-0.47992343148315886,0.623664975274248),
vec2(0.17005534751775433,0.36236272558757426),
vec2(-0.13768339416006067,-0.12466468683535963),
vec2(-0.22739737689046619,0.06925135113784092),
vec2(-0.8882408203885395,1.3152845893183636),
vec2(1.1617478481501655,-0.7292037435304355),
vec2(-0.4328327870854857,-0.08897420847907281),
vec2(0.925175464557536,-1.2353341007197227),
vec2(0.22092434641503914,-0.24218421735956036),
vec2(-0.08378882319080498,0.4688826029623465),
vec2(-0.3960343115939937,-0.38635770559534494),
vec2(-0.3188924363321132,0.569499921617207),
vec2(-0.03683887035419563,-0.7954311958230854),
vec2(-0.09450419771587501,0.29211954070584534),
vec2(0.4472860895361146,0.19349571995553375),
vec2(-0.25633114623261444,0.28487918134194906),
vec2(-0.12280094928456349,0.3204329871763139),
vec2(0.16391254412749856,-0.8794755349149822),
vec2(-1.7570749612268997,-0.151549959938219),
vec2(0.10724979032997696,0.06095876945371348),
vec2(0.009113561846803876,0.0177785752668782),
vec2(-0.576086764210254,0.816164821332195),
vec2(0.16604803851262298,0.9177236777853279),
vec2(0.04414360269414039,-0.16542948219307047),
vec2(-0.1351788220966838,0.01857177084234063),
vec2(-0.7092085465078942,0.39183901231627294),
vec2(0.06901566122302026,-0.00981960351572157),
vec2(-0.3391328500844212,0.3217324956489491),
vec2(0.041048967489433014,-0.0254415262044393),
vec2(0.4053245473742547,1.9172450885106358),
vec2(-0.92996481720399,2.164963450779785),
vec2(0.5196652640450755,0.11863577598729791),
vec2(0.24449573551831702,-1.3775060964134471),
vec2(0.21915801324061276,-1.2677540125221953),
vec2(-0.09602157086503126,-0.08779019264353179),
vec2(0.416181565700275,0.07073498236285199),
vec2(-0.8618812226712945,-0.04840766117526334),
vec2(-0.1998672179075647,0.6175198653309043),
vec2(0.22809959649461126,1.366982412660401),
vec2(-0.34526867018845986,0.005894151076643104),
vec2(-1.2624617783612047,1.2649560375444588),
vec2(-0.19687678967210917,1.1499731482324775),
vec2(0.04441099489338395,0.6027943541523875),
vec2(-0.3750579364476125,0.16167134075396097),
vec2(-0.047266995457563334,0.7563692049342606),
vec2(-0.59190675918357,-0.06555525371076065),
vec2(-0.464420109139586,-0.3410772313241564),
vec2(0.29411014225208104,0.3943208444769222),
vec2(1.5364495734355959,-1.4249354708524355),
vec2(0.40717028542884454,-0.3880500723746609),
vec2(0.7359607466403938,0.4784260047094088),
vec2(0.6552083339909127,0.06278708044377167),
vec2(-0.19256501623895855,-0.13007538769231886),
vec2(-0.2729344986165761,-1.2028610621333324),
vec2(0.4578028075995476,0.7686739493859727),
vec2(0.0010170148292711148,-0.00471257204245161),
vec2(0.519247481446171,1.4721876110065615),
vec2(0.8077495688626751,-0.6387270448534859),
vec2(-0.04314083491039442,0.016934042985244136),
vec2(0.003283691643616376,0.21485114199400154)
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

void PointLightStrength(
    in vec3 wsLightPosition,
    in vec3 wsPosition,
    in vec3 wsCameraPosition,
    in vec3 wsNormal,
    out float diffuseStrength,
    out float diffuseBackStrength,
    out float specularStrength
) {
    vec3 wsToCamera = normalize(wsCameraPosition-wsPosition);
    vec3 wsToLight = (wsLightPosition - wsPosition);
    float distanceToLight = length(wsToLight);
    wsToLight = normalize(wsToLight);

    float attenuation = AttenuateLight(distanceToLight);

    // Diffuse
    float lambert = max(0, dot(wsToLight, wsNormal));
    diffuseStrength= lambert * attenuation;
    float lambertBack = max(0, dot(wsToLight, -wsNormal));
    diffuseBackStrength= lambertBack * attenuation;

    // Specular
    vec3 halfway = normalize(wsToLight + wsToCamera);
    float align = max(0, dot(halfway, wsNormal));
    float shininess = pow(align, 32);
    specularStrength= shininess * attenuation;   
}

void main() {
    Color.rgb = vec3(0);
    Color.a = 1;
 
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

    vec3 wsToCamera = normalize(CameraPosition - wsPosition);

    // Accumulate the various lights
    // (A global ambient light)
    Color.rgb += AmbientLight.rgb * diffuse;

    for (int i=0; i<LightCount; ++i) {
        float d, db, s;
        PointLightStrength(Lights[i].Position, wsPosition,
            CameraPosition,
            wsNormal,
            d, db, s);
        Color.rgb += d * diffuse * Lights[i].Color;
        Color.rgb += db * diffuse * Lights[i].Color * translucency;
        Color.rgb += s * specular * Lights[i].Color;            
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
        float f = cutoffFactor * shadowFactor;

        float d,db,s;
        PointLightStrength(FlashlightPosition, wsPosition,
            CameraPosition,
            wsNormal,
            d, db, s);
        Color.rgb += d * diffuse * FlashlightColor * f;
        Color.rgb += db * diffuse * FlashlightColor * translucency * f;
        Color.rgb += s * specular * FlashlightColor * f;    
    }

    {
        vec4 lsPosition = ShadowmapVPMat * vec4(wsPosition,1);
        lsPosition.xyz /= lsPosition.w; // Perspective divide
        float lsFragDepth = (lsPosition.z + 1) / 2;
        vec2 shadowUv = (lsPosition.xy + vec2(1)) / 2;
        const int VPL_COUNT = RSMVPLCount;
        const float samplingRadius = RSMSamplingRadius;
        vec3 indirectLighting = vec3(0);
        if (dot(normalize(wsPosition-FlashlightPosition), normalize(FlashlightDirection)) > 0)
        for (int i=0; i<VPL_COUNT; ++i) {
            // vec2 vplUv = shadowUv + poissonDisk[i] * samplingRadius;
            vec2 vplUv = shadowUv + importanceSample[i] * samplingRadius;
            Light vpl;
            vpl.Position = texture(RSM[RSMPositionBuf], vplUv).rgb;
            vpl.Color = texture(RSM[RSMFluxBuf], vplUv).rgb;
            vec3 vplSurfaceNormal = texture(RSM[RSMNormalBuf], vplUv).rgb;
            float surfOrientation = max(0, -dot(wsNormal, vplSurfaceNormal)); // (why not this??)

            float d, db, s;
            PointLightStrength(FlashlightPosition, vpl.Position, 
                CameraPosition,
                vplSurfaceNormal,
                d, db, s);
            vpl.Color = d * vpl.Color * FlashlightColor * RSMReflectionFact;

            PointLightStrength(vpl.Position, wsPosition,
                CameraPosition,
                wsNormal,
                d, db, s);
            indirectLighting.rgb += d * diffuse * vpl.Color * surfOrientation;
            indirectLighting.rgb += db * diffuse * vpl.Color * translucency * surfOrientation;
            indirectLighting.rgb += s * specular * vpl.Color * surfOrientation;
        }
        if (VisualizeIndirectLighting) {
            Color.rgb = indirectLighting;
            return;
        }
        if (EnableIndirectLighting) {
            Color.rgb += indirectLighting;
        }
    }

    Color.rgb += RaymarchVolumetric(wsPosition+wsNormal*0.05);

    // Gamma correction
    Color.rgb = Gamma_FromLinear( Color.rgb );

    // Tone mapping (Reinhard tone mapping)
    if (Tonemap)
        Color.rgb /= Color.rgb + vec3(1);
}