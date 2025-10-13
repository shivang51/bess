#version 460

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

layout(location = 0) in vec4 v_FragColor;
layout(location = 1) in vec2 v_TexCoord;
layout(location = 2) in float v_InnerRadius;
layout(location = 3) in flat int v_FragId;

void main() {
    vec2 uv = v_TexCoord - 0.5;
    float dist = length(uv);

    float outerR = 0.5;
    float innerR = v_InnerRadius * 0.5;

    float aa = fwidth(length(uv)) * 0.5;

    float outerMask = 1.0 - smoothstep(outerR - aa, outerR + aa, dist);
    float innerMask = smoothstep(innerR - aa, innerR + aa, dist);

    float mask = outerMask * innerMask;

    if (mask < 0.001)
        discard;

    vec4 col = v_FragColor;

    col.a *= mask;
    col.rgb *= col.a;

    fragColor = col;
    fragColor1 = v_FragId;
}
