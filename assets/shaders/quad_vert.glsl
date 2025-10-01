#version 460

layout(location = 0) in vec3 a_LocalPosition;
layout(location = 1) in vec2 a_LocalTexCoord;

layout(location = 2) in vec3 a_InstancePosition;
layout(location = 3) in vec4 a_InstanceColor;
layout(location = 4) in vec4 a_InstanceBorderRadius;
layout(location = 5) in vec4 a_InstanceBorderColor;
layout(location = 6) in vec4 a_InstanceBorderSize;
layout(location = 7) in vec2 a_InstanceSize;
layout(location = 8) in int a_InstanceId;
layout(location = 9) in int a_InstanceIsMica;
layout(location = 10) in int a_InstanceTexSlotIdx;
layout(location = 11) in vec4 a_InstanceTexData;

layout(location = 0) out vec4 v_FragColor;
layout(location = 1) out vec2 v_TexCoord;
layout(location = 2) out vec4 v_BorderRadius;
layout(location = 3) out vec4 v_BorderSize;
layout(location = 4) out vec4 v_BorderColor;
layout(location = 5) out vec2 v_Size;
layout(location = 6) out flat int v_FragId;
layout(location = 7) out int v_IsMica;
layout(location = 8) out flat int v_TexSlotIdx;

layout(binding = 0) uniform UniformBufferObject {
    mat4 u_mvp;
};

void main() {
    vec2 transformedPos = a_LocalPosition.xy * a_InstanceSize;
    vec3 worldPos = a_InstancePosition + vec3(transformedPos, 0.0);

    gl_Position = u_mvp * vec4(worldPos, 1.0);

    v_FragColor = a_InstanceColor;
    v_TexCoord = a_LocalTexCoord;
    v_BorderRadius = a_InstanceBorderRadius;
    v_BorderColor = a_InstanceBorderColor;
    v_BorderSize = a_InstanceBorderSize;
    v_FragId = a_InstanceId;
    // Use instance size in pixels for AA/rounding
    v_Size = a_InstanceSize;
    v_IsMica = a_InstanceIsMica;
    v_TexSlotIdx = a_InstanceTexSlotIdx;
}

