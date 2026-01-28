#version 460

layout(location = 0) in vec2 a_LocalPos;
layout(location = 1) in vec2 a_TexCoord;

layout(location = 2) in vec3 a_InstancePos;
layout(location = 3) in vec4 a_Color;
layout(location = 4) in float a_Radius;
layout(location = 5) in float a_InnerRadius;
layout(location = 6) in uvec2 a_Id;

layout(location = 0) out vec4 v_FragColor;
layout(location = 1) out vec2 v_TexCoord;
layout(location = 2) out float v_InnerRadius;
layout(location = 3) out flat uvec2 v_FragId;

layout(binding = 0) uniform UniformBufferObject {
    mat4 u_mvp;
    mat4 u_ortho;
};

void main() {
    v_FragColor = a_Color;
    v_TexCoord = a_TexCoord;
    v_InnerRadius = a_InnerRadius / a_Radius;
    v_FragId = a_Id;

    vec2 transformedPos = a_LocalPos * vec2(a_Radius * 2.f, a_Radius * 2.f);
    vec3 worldPos = a_InstancePos + vec3(transformedPos, 0.0);

    gl_Position = u_mvp * vec4(worldPos, 1.0);
}
