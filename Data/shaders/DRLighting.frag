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
for i in range(128):
  angle = np.random.uniform(0,2*math.pi)
  length = np.random.normal()
  x=math.cos(angle)
  y=math.sin(angle)
  x*=length
  y*=length
  print(f"vec2({x},{y})",end='')
  if i != 127:
    print(",")
  else:
    print()
print(");")
*/
const vec2 importanceSample[] = vec2[](
vec2(0.578144417323147,-0.1462584719575692),
vec2(-0.4193570104747924,0.4850117239736373),
vec2(0.12225481859502153,-0.19736604993701864),
vec2(0.006238833564889672,-0.5254410319433145),
vec2(0.09363198657213426,-0.09258158313765304),
vec2(-0.4400580603494623,-1.6726310064326468),
vec2(0.54146688704861,-0.5954718273176822),
vec2(0.6491474341653608,-0.4365226734288161),
vec2(0.46343176255130536,0.3012851532796474),
vec2(1.0560317442071387,-0.07230146717559485),
vec2(-1.036791826081943,-0.19418972911654353),
vec2(0.07756334555757191,-0.0316764254897321),
vec2(-0.029490673335838952,0.21519669160587304),
vec2(0.7390891421611004,-0.8630313613063276),
vec2(1.3700814721842687,1.7687578502772463),
vec2(-0.7044030293180534,0.37818237314035213),
vec2(0.47635257934273867,-0.37035554387263925),
vec2(0.34207173675248476,-0.0314483963964922),
vec2(-1.3753141108186053,-0.16752961761625598),
vec2(0.8368002701091563,0.6400860114766372),
vec2(0.2402708767520334,-0.02451873172018978),
vec2(-0.8981559130501067,0.12404249529380866),
vec2(-0.3289575961668495,0.9946887523993538),
vec2(-0.5468689929982541,-0.5833515068446652),
vec2(-0.318078897496897,0.5254459244027219),
vec2(0.04993159472179276,-0.08679492867277279),
vec2(-0.0011816840048337108,0.002678566964963464),
vec2(-0.415517002585894,-0.6237485954461933),
vec2(0.7500093086733572,-0.9837560040544555),
vec2(1.0219770420205117,0.2308566051311198),
vec2(0.25352062840705664,-0.02127716023341058),
vec2(0.2793292491557195,0.2654573001051046),
vec2(-1.3681140908852862,0.7664149040371159),
vec2(-0.3583934317586108,-0.3332180730573257),
vec2(-0.033356332709368275,0.01132995018664298),
vec2(-0.6988904685691023,-0.13776286364978496),
vec2(-0.036633308130040905,-0.35350399896547796),
vec2(0.726145052342095,0.35761250266676514),
vec2(0.6500969049656469,-1.1723702705753072),
vec2(0.14756580104155836,0.3098433595658269),
vec2(0.14332634870635683,-0.26798899498246137),
vec2(-0.9579578303705313,-0.9627681460765465),
vec2(0.4623351856224248,-0.6319542246842678),
vec2(0.41403230829279797,-0.1441414737135078),
vec2(0.0065288221676462034,-0.03931225346509223),
vec2(0.21286702904103186,0.28973138066243037),
vec2(0.22137637632968307,0.17173539115914285),
vec2(-0.5608652262362684,1.2981833473829545),
vec2(0.14817003267540385,-0.2630348453510056),
vec2(-0.3998030792641473,-0.1774402319271183),
vec2(1.4920056278122953,-1.7741560247803119),
vec2(-0.24522294991525012,0.02574962835099858),
vec2(-1.8662983108542681,1.8127459556110672),
vec2(0.06314850982428023,0.11081000016968966),
vec2(-1.1928195012118337,1.1242782809120293),
vec2(1.218434673687453,-0.4512699011189052),
vec2(0.05504471592848078,0.14226325234369205),
vec2(1.212307920706111,-0.7555858607058746),
vec2(-0.00017450914312631887,0.00041656519262302965),
vec2(0.04350695297211408,-0.5358076619353707),
vec2(-0.3853134116983415,0.013685007215969405),
vec2(0.030251571556091185,-0.038213655350437255),
vec2(-0.7319897486523007,0.0022592939643643743),
vec2(0.6107455932068113,0.47353186054105806),
vec2(0.031414705710580956,0.5177913087355348),
vec2(-0.0645278093334358,-0.06981183724451158),
vec2(0.4388606070989726,0.012714352654643216),
vec2(-0.1859323389848128,-0.05572079082480194),
vec2(-1.5855580446882942,-0.7466289784506924),
vec2(-0.22098444660696523,0.8805870224902342),
vec2(-0.7370113866428316,-0.3334593216085183),
vec2(-0.6186154397394844,-0.18897732375561738),
vec2(0.6218844641771294,-1.1071202077541022),
vec2(0.6867145020029892,-0.9818129445265935),
vec2(-0.8133856169893867,0.23020259533584808),
vec2(-0.4084226529362702,-0.6805397148322024),
vec2(-0.5971788512168102,1.077493248614435),
vec2(-0.010733213598247174,0.01264917624524064),
vec2(0.412748718752631,0.09759912090103218),
vec2(-0.17342869074317524,-0.55033622559879),
vec2(-0.03360516719355684,-0.18617604733250234),
vec2(0.8132414057842039,-0.7265648399054564),
vec2(0.2276336259663251,-0.02878815046965735),
vec2(1.1208807931767384,0.6782727690953754),
vec2(0.0701066440406942,-0.15516305382063372),
vec2(0.3098964143661185,0.08869749853526976),
vec2(0.5965464431939882,-2.2261148200339145),
vec2(0.4655834880257976,1.1612989265116922),
vec2(0.1323155321265021,-0.2953384827190456),
vec2(-0.5802626561668853,-1.4609529282155636),
vec2(0.6338425329077793,0.07626704526820767),
vec2(0.3854239680519026,-0.015481404827304479),
vec2(-0.025047998383939663,0.5629230284585491),
vec2(0.3742448784840041,-0.028586566966205814),
vec2(0.09622044114871442,0.15072168284432386),
vec2(-0.023026196401661387,-0.17469932330019386),
vec2(-0.017465022052521588,0.6148798112374525),
vec2(-0.18612123017871643,-0.9642311414741397),
vec2(1.4813587876625152,-0.42203879910148695),
vec2(0.21902268697525942,-1.0567965511836641),
vec2(0.24028574416348397,1.379508018987801),
vec2(0.28356938654003017,0.7768409363589603),
vec2(0.09225328901072488,0.12101104237745225),
vec2(-0.20937279492103017,0.15424809938860323),
vec2(-0.6229808699626134,0.1851992809679111),
vec2(0.08592337216750835,0.13167552193119136),
vec2(-1.3781696754795687,0.16722299669936713),
vec2(0.031939729657158064,-0.16108203776234867),
vec2(-1.0807196239275976,0.9142734272451112),
vec2(0.06749749708986913,0.052340086207806065),
vec2(0.672239391217113,-0.06259681160654855),
vec2(-0.08571132058322369,0.02203976706835423),
vec2(0.06293844401758457,-0.07999594294330514),
vec2(-0.25770230742518224,0.5860943699828771),
vec2(-0.06911596441835423,0.17346553481369265),
vec2(0.07819511597443478,0.06908657512403621),
vec2(-0.3497290166000055,-0.4241800657212301),
vec2(1.0055713868906861,-0.09118393118959002),
vec2(0.008620593461105958,0.11520874764339603),
vec2(0.5532919271339946,-0.2238375108924985),
vec2(0.043410213935094755,0.22991472454407144),
vec2(0.4153827177059009,-0.4302438847331684),
vec2(-0.08589540811903972,-0.04848110213300951),
vec2(-0.18714422106008494,-0.07729853682895979),
vec2(0.45211904406439435,0.39902356325453003),
vec2(0.176550773127993,0.26830737350059636),
vec2(0.46587558138643115,0.3621369560228713),
vec2(0.49967226283654603,-0.4497662880810977)
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