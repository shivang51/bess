#version 460 core

layout(location = 0) in vec3 a_Vertex;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in int a_FragId;
layout(location = 4) in int a_TexSlotIdx;

out vec3 v_FragPos;
out vec4 v_FragColor;
out vec2 v_TexCoord;
out flat int v_FragId;
out flat int v_TexSlotIdx;

uniform mat4 u_mvp;

void main() {
    v_FragPos = a_Vertex;
    v_FragColor = a_Color;
    v_TexCoord = a_TexCoord;
    v_FragId = a_FragId;
    v_TexSlotIdx = a_TexSlotIdx;

    gl_Position = u_mvp * vec4(a_Vertex, 1.0);
}
