#version 460

layout(location = 0) in vec3 a_Vertex;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in uvec2 a_FragId;
layout(location = 3) in vec4 a_FragColor;
layout(location = 4) in int a_AR;

layout(location = 1) out vec2 v_TexCoord;
layout(location = 2) out flat uvec2 v_FragId;
layout(location = 4) out vec4 v_FragColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 u_mvp;
    mat4 u_ortho;
};

void main() {
    v_TexCoord = a_TexCoord;
    v_FragId = a_FragId;
    v_FragColor = a_FragColor;

    vec4 orthoPos = u_ortho * vec4(a_Vertex.xy, 0.0, 1.0);
    vec4 transformedZ = u_mvp * vec4(0.0, 0.0, a_Vertex.z, 1.0);

    gl_Position = vec4(orthoPos.xy, transformedZ.z, orthoPos.w);
}
