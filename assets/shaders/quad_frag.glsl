#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec3 v_FragPos;
in vec2 v_Size;
in vec2 v_TexCoord;
in vec3 v_FragColor;
in float v_BorderSize;
in vec4 v_BorderRadius;
in vec4 v_BorderColor;
in flat int v_FragId;

uniform int u_SelectedObjId;

float sdRoundBox(in vec2 p, in vec2 b, in vec4 r)
{
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x = (p.y > 0.0) ? r.x : r.y;
    vec2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}

void main() {
    float ar = v_Size.x / v_Size.y;
    vec2 p = v_TexCoord;
    float bS = v_BorderSize;
    vec3 borderColor = v_BorderColor.xyz;
    vec3 bgColor = v_FragColor;
    vec4 bR = v_BorderRadius;

    float smoothBlur = 0.008f;
    vec4 ra = vec4(bR.y, bR.z, bR.x, bR.w);

    p = (p * 2.f) - 1.f;
    p.x *= ar;
    vec2 si = vec2(ar, 1.f);
    float d = sdRoundBox(p, si, ra);
    float a = smoothstep(smoothBlur, 0.0f, d);
    if (a == 0.f) discard;

    vec3 col = borderColor * a;

    si -= bS;
    ra -= bS;

    d = sdRoundBox(p, si, ra);
    a = smoothstep(smoothBlur, 0.0f, d);
    col = mix(col, bgColor, a);
    fragColor = vec4(col, 1.0);

    fragColor = vec4(v_FragPos.x, 0.f, 0.f, 1.f);
    fragColor1 = v_FragId;
}
