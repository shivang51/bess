#version 450

layout(binding = 0) uniform sampler2D u_Input;
layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 o_Color;

void main() {
    vec4 c = texture(u_Input, v_TexCoord);
    if (c.a > 0.0001)
        c.rgb /= c.a;
    o_Color = c;
}
