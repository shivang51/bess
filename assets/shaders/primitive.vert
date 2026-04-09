#version 460

layout(location = 0) in vec2 a_LocalPosition;
layout(location = 1) in vec2 a_LocalTexCoord;

layout(location = 2) in vec3 a_InstancePosition;
layout(location = 3) in vec4 a_InstanceColor;
layout(location = 4) in vec4 a_InstanceBorderRadius;
layout(location = 5) in vec4 a_InstanceBorderColor;
layout(location = 6) in vec4 a_InstanceBorderSize;
layout(location = 7) in vec4 a_InstanceTexData;
layout(location = 8) in vec4 a_InstancePrimitiveData;
layout(location = 9) in vec2 a_InstanceSize;
layout(location = 10) in uvec2 a_InstanceId;
layout(location = 11) in int a_InstancePrimitiveType;
layout(location = 12) in int a_InstanceIsMica;
layout(location = 13) in int a_InstanceTexSlotIdx;
layout(location = 14) in float a_InstanceAngle;

layout(location = 0) out vec4 v_FragColor;
layout(location = 1) out vec2 v_TexCoord;
layout(location = 2) out vec2 v_LocalCoord;
layout(location = 3) out vec4 v_BorderRadius;
layout(location = 4) out vec4 v_BorderSize;
layout(location = 5) out vec4 v_BorderColor;
layout(location = 6) out vec2 v_Size;
layout(location = 7) out vec4 v_PrimitiveData;
layout(location = 8) out flat uvec2 v_FragId;
layout(location = 9) out flat int v_IsMica;
layout(location = 10) out flat int v_TexSlotIdx;
layout(location = 11) out flat int v_PrimitiveType;

layout(binding = 0) uniform UniformBufferObject {
    mat4 u_mvp;
    mat4 u_ortho;
};

void main() {
    vec2 scaledPos = a_LocalPosition * a_InstanceSize;

    float cosAngle = cos(a_InstanceAngle);
    float sinAngle = sin(a_InstanceAngle);
    vec2 rotatedPos = vec2(
        scaledPos.x * cosAngle - scaledPos.y * sinAngle,
        scaledPos.x * sinAngle + scaledPos.y * cosAngle
    );

    vec3 worldPos = a_InstancePosition + vec3(rotatedPos, 0.0);
    gl_Position = u_mvp * vec4(worldPos, 1.0);

    vec2 texStart = a_InstanceTexData.xy;
    vec2 texSize = a_InstanceTexData.zw;

    v_FragColor = a_InstanceColor;
    v_TexCoord = texStart + (texSize * a_LocalTexCoord);
    v_LocalCoord = a_LocalPosition;
    v_BorderRadius = a_InstanceBorderRadius;
    v_BorderColor = a_InstanceBorderColor;
    v_BorderSize = a_InstanceBorderSize;
    v_Size = a_InstanceSize;
    v_PrimitiveData = a_InstancePrimitiveData;
    v_FragId = a_InstanceId;
    v_IsMica = a_InstanceIsMica;
    v_TexSlotIdx = a_InstanceTexSlotIdx;
    v_PrimitiveType = a_InstancePrimitiveType;
}
