#version 460

// Vertex attributes (per-vertex data)
layout(location = 0) in vec3 a_Vertex;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in int a_FragId;
layout(location = 4) in int a_TexSlotIdx;

// Instance attributes (per-instance data)
layout(location = 5) in vec3 a_InstancePosition;
layout(location = 6) in vec3 a_InstanceScale;
layout(location = 7) in vec4 a_InstanceColor;
layout(location = 8) in int a_InstanceId;

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
    // Apply instance transformations
    vec3 worldPos = a_Vertex * a_InstanceScale + a_InstancePosition;
    
    // Use instance color if provided, otherwise use vertex color
    vec4 finalColor = a_InstanceColor.w > 0.0 ? a_InstanceColor : a_Color;
    
    // Use instance ID if provided, otherwise use vertex ID
    int finalId = a_InstanceId != 0 ? a_InstanceId : a_FragId;
    
    v_FragPos = worldPos;
    v_FragColor = finalColor;
    v_TexCoord = a_TexCoord;
    v_FragId = finalId;
    v_TexSlotIdx = a_TexSlotIdx;

    gl_Position = u_mvp * vec4(worldPos, 1.0);
}
