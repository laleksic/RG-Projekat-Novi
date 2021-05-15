uniform mat4 MVPMat;
uniform mat4 ModelMat;
uniform vec3 LightColor;

#if defined(VERTEX_SHADER)
    layout (location=0) in vec3 Position;

    void main() {
        gl_Position.xyz = Position;
        gl_Position.w = 1.0f;
        gl_Position = MVPMat * gl_Position;
    }
#elif defined(FRAGMENT_SHADER)
    out vec4 Color;

    void main() {
        Color.rgb = LightColor.rgb;
        Color.a = 1;
    }
#endif