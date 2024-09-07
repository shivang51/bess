#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec2 v_Size;
in vec2 v_TexCoord;
in vec4 v_FragColor;
in vec4 v_BorderRadius;
in flat int v_FragId;

uniform int u_SelectedObjId;
uniform float u_zoom;


void main() {
    vec2 fragPos = gl_FragCoord.xy;
    float ar = v_Size.x / v_Size.y;
    vec2 p = v_TexCoord;
    vec4 bgColor = vec4(0.0, 0.0, 0.0, 1.);

    p = (p * 2.f) - 1.f;
    p.y += 0.1f;

    // float a = smoothstep(1.5f, 0.5, length(p));;
    float a = 1.f - distance(p,  vec2(clamp(p.x, -0.4, 0.4), 0.));  

    fragColor = bgColor * a;
    fragColor1 = v_FragId;
}
