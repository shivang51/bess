#version 460 core

layout(location = 0) in vec2 a_LocalPos;
layout(location = 1) in vec2 a_TexCoord;

layout(location = 2) in vec3 a_InstancePos;
layout(location = 3) in vec4 a_Color;
layout(location = 4) in float a_Radius;
layout(location = 5) in float a_InnerRadius;
layout(location = 6) in int a_TextureIndex;

out vec4 v_FragColor;
out vec2 v_TexCoord;
out float v_InnerRadius;
out flat int v_TextureIndex;

uniform mat4 u_mvp;

void main() {
    v_FragColor = a_Color;
    v_TexCoord = a_TexCoord;
    v_InnerRadius = a_InnerRadius / a_Radius;
    v_TextureIndex = a_TextureIndex;

    vec2 transformedPos = a_LocalPos * vec2(a_Radius * 2.f, a_Radius * 2.f);
    vec3 worldPos = a_InstancePos + vec3(transformedPos, 0.0);

    gl_Position = u_mvp * vec4(worldPos, 1.0);
}
