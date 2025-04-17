#version 460 core

layout(location = 0) in vec3 a_Vertex;
layout(location = 1) in vec2 a_TexCoord;

out vec2 v_TexCoord;

uniform mat4 u_mvp;

void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = u_mvp * vec4(a_Vertex + vec3(-4.0, 4.0, 0.0), 1.0);
}
