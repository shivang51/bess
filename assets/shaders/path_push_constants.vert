#version 460

// Vertex attributes
layout(location = 0) in vec3 a_Vertex;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in int a_FragId;
layout(location = 4) in int a_TexSlotIdx;

// Output to fragment shader
layout(location = 0) out vec3 v_FragPos;
layout(location = 1) out vec4 v_FragColor;
layout(location = 2) out vec2 v_TexCoord;
layout(location = 3) out flat int v_FragId;
layout(location = 4) out flat int v_TexSlotIdx;

// Uniform buffer
layout(binding = 0) uniform UniformBufferObject {
    mat4 u_mvp;
    mat4 u_ortho;
};

// Push constants (two vec4s for alignment)
layout(push_constant) uniform PushConstants {
    vec4 translation; // xyz used
    vec4 scale;       // xy used (uniform scale if both equal)
} pc;

void main() {
    vec2 scaledXY = a_Vertex.xy * pc.scale.xy;
    vec3 worldPos = vec3(scaledXY, a_Vertex.z) + pc.translation.xyz;

    v_FragPos = worldPos;
    v_FragColor = a_Color;
    v_TexCoord = a_TexCoord;
    v_FragId = a_FragId;
    v_TexSlotIdx = a_TexSlotIdx;

    gl_Position = u_mvp * vec4(worldPos, 1.0);
}
