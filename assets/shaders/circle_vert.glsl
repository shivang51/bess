#version 460 core

layout(location = 0) in vec3 a_Vertex;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in float a_InnerRadius;
layout(location = 4) in int a_TextureIndex;

out vec3 v_FragPos;
out vec4 v_FragColor;
out vec2 v_TexCoord;
out float v_InnerRadius;
out flat int v_TextureIndex;

uniform mat4 u_mvp;

void main() {
    v_FragPos = a_Vertex;
    v_FragColor = a_Color;
    v_TexCoord = a_TexCoord;
    v_InnerRadius = a_InnerRadius;
    v_TextureIndex = a_TextureIndex;

    gl_Position = u_mvp * vec4(a_Vertex, 1.0);
}
