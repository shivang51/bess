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

float smoothBlur = 0.035f;

float sdRoundBox(in vec2 p, in vec2 b, in vec4 r)
{
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x = (p.y > 0.0) ? r.x : r.y;
    vec2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}

float calculateQuad(vec2 p, vec2 si, vec4 ra){
    float d = sdRoundBox(p, si, ra);
    float a = smoothstep(smoothBlur, 0.0f, d);
    return a;
}

float minVec2(vec2 v){
    return (v.x <= v.y) ? v.x : v.y;  
}

void main() {
    vec2 fragPos = gl_FragCoord.xy;
    float ar = v_Size.x / v_Size.y;
    vec2 p = v_TexCoord;
    vec4 bgColor = v_FragColor;
    vec4 bR = v_BorderRadius;
    bR /= minVec2(v_Size);

    smoothBlur /= u_zoom;

    p = (p * 2.f) - 1.f;
    p.x *= ar;
    vec2 si = vec2(ar, 1.f);

    vec4 ra = vec4(bR.y, bR.z, bR.x, bR.w);
    
    float a = calculateQuad(p, si, ra);
    if (a == 0.f) discard;
    bgColor.w = min(bgColor.w, a);
    fragColor = bgColor;
    fragColor1 = v_FragId;
}
