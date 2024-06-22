

#version 460 core

layout(location = 0) in vec3 a_Vertex;
layout(location = 1) in vec3 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in float a_BorderSize;
layout(location = 4) in vec4 a_BorderRadius;
layout(location = 5) in vec4 a_BorderColor;
layout(location = 6) in vec2 a_Size;
layout(location = 7) in int a_FragId;

out vec3 v_FragPos;
out vec3 v_FragColor;
out vec2 v_TexCoord;
out float v_BorderSize;
out vec4 v_BorderRadius;
out vec4 v_BorderColor;
out vec2 v_Size;
out flat int v_FragId;

uniform mat4 u_mvp;

void main() {
    vec4 pos = u_mvp * vec4(a_Vertex, 1.0);
    v_FragPos = pos.xyz;
    v_FragColor = a_Color;
    v_TexCoord = a_TexCoord;
    v_BorderSize = a_BorderSize;
    v_BorderRadius = a_BorderRadius;
    v_BorderColor = a_BorderColor;
    v_FragId = a_FragId;
    v_Size = a_Size;

    gl_Position = pos;
}
