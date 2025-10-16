#version 450

layout(location = 0) out vec2 v_TexCoord;

void main() {
    vec2 pos;
    if (gl_VertexIndex == 0) { // Bottom-left
        pos = vec2(-1.0, -1.0);
    } else if (gl_VertexIndex == 1) { // Bottom-right
        pos = vec2(1.0, -1.0);
    } else if (gl_VertexIndex == 2) { // Top-left
        pos = vec2(-1.0, 1.0);
    } else { // gl_VertexIndex == 3, Top-right
        pos = vec2(1.0, 1.0);
    }
    
    gl_Position = vec4(pos, 0.0, 1.0);
    
    // Generate UV coordinates
    v_TexCoord = pos * 0.5 + 0.5;
}
