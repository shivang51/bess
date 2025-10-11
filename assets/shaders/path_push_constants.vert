#version 460

// Vertex attributes
layout(location = 0) in vec3 a_Vertex;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in int a_FragId;
layout(location = 4) in int a_TexSlotIdx;
// Instance attributes
layout(location = 5) in vec2 a_InstanceTranslation;
layout(location = 6) in vec2 a_InstanceScale;
layout(location = 7) in int a_InstancePickId;

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

void main() {
    vec2 scaledXY = a_Vertex.xy * a_InstanceScale;
    vec3 worldPos = vec3(scaledXY + a_InstanceTranslation, a_Vertex.z);

    v_FragPos = worldPos;
    v_FragColor = a_Color;
    v_TexCoord = a_TexCoord;
    // Use instance-level pick id for picking; falls back to vertex id if not set
    v_FragId = (a_InstancePickId != 0) ? a_InstancePickId : a_FragId;
    v_TexSlotIdx = a_TexSlotIdx;

    gl_Position = u_mvp * vec4(worldPos, 1.0);
}
