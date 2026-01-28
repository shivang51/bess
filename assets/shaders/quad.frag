#version 460

layout(location = 0) out vec4 fragColor;
layout(location = 1) out uvec2 pickingId;

layout(location = 0) in vec4 v_FragColor;
layout(location = 1) in vec2 v_TexCoord;
layout(location = 2) in vec4 v_BorderRadius;
layout(location = 3) in vec4 v_BorderSize;
layout(location = 4) in vec4 v_BorderColor;
layout(location = 5) in vec2 v_Size;
layout(location = 6) in flat uvec2 v_FragId;
layout(location = 7) in flat int v_IsMica;
layout(location = 8) in flat int v_TexSlotIdx;

// Per-texture sampler bound in set 1, binding 2
layout(set = 1, binding = 2) uniform sampler2D uTextures[32];

float sdRR(vec2 p, vec2 b, vec4 r) {
    float rc = p.x >= 0.0 && p.y >= 0.0 ? r.y :
        p.x < 0.0 && p.y >= 0.0 ? r.x :
        p.x >= 0.0 && p.y < 0.0 ? r.z : r.w;
    vec2 d = abs(p) - (b - vec2(rc));
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - rc;
}

void main() {
    vec2 uv = v_TexCoord - 0.5;
    uv.x *= v_Size.x / v_Size.y;

    vec2 oH = vec2((v_Size.x / v_Size.y) * 0.5, 0.5);
    float aa = fwidth(length(uv));

    vec4 oR = v_BorderRadius / v_Size.y;
    vec4 borderSize = v_BorderSize / v_Size.y;

    float dO = sdRR(uv, oH, oR);

    float L = -oH.x, R = oH.x, B = -oH.y, T = oH.y;
    float iL = L + borderSize.w, iR = R - borderSize.y, iB = B + borderSize.z, iT = T - borderSize.x;
    vec2 iC = vec2((iL + iR) * 0.5, (iB + iT) * 0.5);
    vec2 iH = vec2((iR - iL) * 0.5, (iT - iB) * 0.5);

    vec4 iRads = vec4(
            max(0.0, oR.x - min(borderSize.x, borderSize.w)),
            max(0.0, oR.y - min(borderSize.x, borderSize.y)),
            max(0.0, oR.z - min(borderSize.z, borderSize.y)),
            max(0.0, oR.w - min(borderSize.z, borderSize.w))
        );

    float dI = sdRR(uv - iC, iH, iRads);

    float mO = smoothstep(aa, -aa, dO);
    float mI = smoothstep(aa, -aa, dI);
    if (mO < 0.001) discard;

    vec4 baseColor = v_FragColor;
    if (v_TexSlotIdx != 0) {
        vec4 texColor = texture(uTextures[v_TexSlotIdx], v_TexCoord);
        baseColor *= texColor;
    }

    if (v_IsMica == 1) {
        vec3 dark = baseColor.rgb * smoothstep(0.5f, 1.f, length(1.f - v_TexCoord - 0.45f));

        float v = length(vec2(v_TexCoord.x, v_TexCoord.y + 0.01f));
        baseColor.rgb *= mix(baseColor.rgb, dark, v);
        baseColor.a *= mix(0.4f, 0.9f, v);
    }

    vec4 color = mix(v_BorderColor, baseColor, mI);
    color.a *= mO;

    if (color.a < 0.001) discard;

    // Premultiply RGB by alpha for proper blending
    color.rgb *= color.a;

    fragColor = color;
    pickingId = v_FragId;
}
