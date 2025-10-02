#version 460

layout(location = 0) in vec3 a_Vertex;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in int a_FragId;
layout(location = 3) in vec4 a_FragColor;
layout(location = 4) in int a_AR;

layout(location = 1) out vec2 v_TexCoord;
layout(location = 2) out flat int v_FragId;
layout(location = 4) out vec4 v_FragColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 u_mvp;
    mat4 u_ortho;
};

void main() {
    v_TexCoord = a_TexCoord;
    v_FragId = a_FragId;
    v_FragColor = a_FragColor;

    gl_Position = u_ortho    * vec4(a_Vertex, 1.0);
}
