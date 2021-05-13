uniform mat4 ModelViewProjection;
uniform sampler2D DiffuseTexture;

#if defined(VERTEX_SHADER)
    layout (location=0) in vec3 Position;
    layout (location=1) in vec3 Color;
    layout (location=2) in vec2 TexCoords;
    out VertexData {
        vec3 Color;
        vec2 TexCoords;
    } vertexData;

    void main() {
        gl_Position.xyz = Position;
        gl_Position.w = 1.0f;
        gl_Position = ModelViewProjection * gl_Position;
        vertexData.Color = Color;
        vertexData.TexCoords = TexCoords;
    }
#elif defined(FRAGMENT_SHADER)
    in VertexData {
        vec3 Color;
        vec2 TexCoords;
    } vertexData;

    out vec4 Color;

    void main() {
        vec4 color = texture(DiffuseTexture, vertexData.TexCoords);
        color *= vec4(vertexData.Color, 1);
        if (color.a < 0.5) {
            discard;
        }
        Color = color;
    }
#endif