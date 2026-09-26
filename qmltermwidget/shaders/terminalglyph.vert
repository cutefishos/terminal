#version 440

layout(location = 0) in vec4 qt_Vertex;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec4 color;

layout(location = 0) out vec2 v_texCoord;
layout(location = 1) out vec4 v_color;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};

out gl_PerVertex { vec4 gl_Position; };

void main()
{
    v_texCoord = texCoord;
    v_color = color * qt_Opacity;
    gl_Position = qt_Matrix * qt_Vertex;
}
