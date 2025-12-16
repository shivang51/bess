#version 460

layout(location = 0) in vec3 a_Vertex;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in uvec2 a_FragId;
layout(location = 4) in int a_TexSlotIdx;

layout(location = 5) in vec3 a_InstancePosition;
layout(location = 6) in vec2 a_InstanceScale;
layout(location = 7) in vec4 a_InstanceColor;
layout(location = 8) in uvec2 a_InstanceId;

layout(location = 0) out vec3 v_FragPos;
layout(location = 1) out vec4 v_FragColor;
layout(location = 2) out vec2 v_TexCoord;
layout(location = 3) out flat uvec2 v_FragId;
layout(location = 4) out flat int v_TexSlotIdx;
layout(location = 5) noperspective out vec3 v_Barycentric;

layout(binding = 0) uniform UniformBufferObject {
    mat4 u_mvp;
    mat4 u_ortho;
};

void main() {
    vec3 worldPos = a_Vertex * vec3(a_InstanceScale, 1.f) + a_InstancePosition;

    v_FragPos = worldPos;
    v_FragColor = a_InstanceColor;
    v_TexCoord = a_TexCoord;
    v_FragId = a_InstanceId;
    v_TexSlotIdx = a_TexSlotIdx;
    int vidx = gl_VertexIndex % 3;
    if (vidx == 0)
        v_Barycentric = vec3(1.0, 0.0, 0.0);
    else if (vidx == 1)
        v_Barycentric = vec3(0.0, 1.0, 0.0);
    else
        v_Barycentric = vec3(0.0, 0.0, 1.0);

    gl_Position = u_mvp * vec4(worldPos, 1.0);
}
