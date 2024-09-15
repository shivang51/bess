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

float sdRoundedBox( in vec2 p, in vec2 b, in vec4 r )
{
    r.xy = (p.x>0.0)?r.xy : r.zw;
    r.x  = (p.y>0.0)?r.x  : r.y;
    vec2 q = abs(p)-b+r.x;
    return min(max(q.x,q.y),0.0) + length(max(q,0.0)) - r.x;
}

void main() {
    float ar = v_Size.x / v_Size.y;
    vec2 p = v_TexCoord;
    vec4 bgColor = vec4(0.0, 0.0, 0.0, 1.);
    
    p = (p * 2.f) - 1.f;
    p.x *= ar;

    vec4 br = v_BorderRadius / v_Size.y;

    float a = 1.f - sdRoundedBox(p, vec2(ar, 1.f) * 0.9f, br);
    a = smoothstep(0.8, 1.f, a);

    fragColor = bgColor * a;
    fragColor1 = v_FragId;
}
