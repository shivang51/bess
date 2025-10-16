#version 460

layout(location = 0) out vec4 fragColor; // premultiplied RGBA
layout(location = 1) out int fragColor1; // picking / ID buffer

layout(location = 0) in vec4 v_FragColor;
layout(location = 1) in vec2 v_TexCoord;
layout(location = 2) in flat int v_FragId;
layout(location = 3) in flat int v_TexSlotIdx;

layout(set = 1, binding = 2) uniform sampler2D u_Textures[32];
layout(binding = 1) uniform TextUniforms {
    float u_pxRange; // same value as --pxrange in msdf-atlas-gen
};

float median3(vec3 v) {
    return max(min(v.r, v.g), min(max(v.r, v.g), v.b));
}

void main()
{
    if (v_TexSlotIdx == 0) {
        fragColor = v_FragColor;
        fragColor1 = v_FragId;
        return;
    }

    // ---- sample base MSDF ----
    vec4 tex = texture(u_Textures[v_TexSlotIdx], v_TexCoord);
    float sd = median3(tex.rgb);
    float fallback = tex.a - 0.5; // for MTSDF

    // ---- screen-space scale (in pixels) ----
    vec2 texSize = vec2(textureSize(u_Textures[v_TexSlotIdx], 0));
    vec2 dx = dFdx(v_TexCoord) * texSize;
    vec2 dy = dFdy(v_TexCoord) * texSize;
    float scale = max(length(dx), length(dy));
    scale = max(scale, 1e-5);

    // ---- convert distance to screen-space ----
    float dist = (sd - 0.5) * (u_pxRange / scale);
    float distFallback = fallback * (u_pxRange / scale);
    dist = mix(distFallback, dist, 0.9); // mostly MSDF, fallback for small text

    // ---- adaptive AA window ----
    float aaWidth = mix(0.9, 1.4, clamp(scale * 0.5, 0.0, 1.0));

    // ---- base alpha with perceptual gamma ----
    float a = smoothstep(-aaWidth, aaWidth, dist);
    float gamma = 1.4;
    a = pow(a, 1.0 / gamma);

    // ---- 2×2 local super-sample (helps diagonals & curves) ----
    vec2 offs = vec2(0.15) / texSize;
    vec3 ms1 = texture(u_Textures[v_TexSlotIdx], v_TexCoord + offs).rgb;
    vec3 ms2 = texture(u_Textures[v_TexSlotIdx], v_TexCoord - offs).rgb;
    float d1 = (median3(ms1) - 0.5) * (u_pxRange / scale);
    float d2 = (median3(ms2) - 0.5) * (u_pxRange / scale);
    float a1 = pow(smoothstep(-aaWidth, aaWidth, d1), 1.0 / gamma);
    float a2 = pow(smoothstep(-aaWidth, aaWidth, d2), 1.0 / gamma);
    a = (a + a1 + a2) / 3.0;

    // ---- premultiplied alpha output ----
    vec4 outColor = vec4(v_FragColor.rgb * a, v_FragColor.a * a);

    if (outColor.a < 0.001)
        discard;

    fragColor = outColor;
    fragColor1 = v_FragId;
}
