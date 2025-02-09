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

float rand(vec2 uv) {
    return fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 fragPos = gl_FragCoord.xy;
    float ar = v_Size.x / v_Size.y;
    vec2 p = v_TexCoord;
    vec4 bgColor = v_FragColor;
    vec4 bR = v_BorderRadius;
    bR /= minVec2(v_Size);

    smoothBlur = fwidth(length(p));

    p = (p * 2.f) - 1.f;
    p.x *= ar;
    vec2 si = vec2(ar, 1.f);

    vec4 ra = vec4(bR.w, bR.x, bR.z, bR.y);
    
    float a = calculateQuad(p, si, ra);
    if (a == 0.f) discard;
    bgColor.w = min(bgColor.w, a);

    float noise = rand(p * 100.f) * 0.01f;
    vec4 tintColor = vec4(0.2, 0.2, 0.3, 0.7);
    vec4 micaColor = mix(bgColor, tintColor, tintColor.a);
    micaColor.rgb += noise; // Add subtle grain
    micaColor.rgb *= 0.3f; // Apply vignette

    fragColor = micaColor;
    fragColor1 = v_FragId;
}
