#version 460

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

layout(location = 0) in vec4 v_FragColor;
layout(location = 1) in vec2 v_TexCoord;
layout(location = 2) in float v_InnerRadius;
layout(location = 3) in flat int v_FragId;

void main() {
    vec2 uv = (v_TexCoord - 0.5);
    float r = 0.5f;
    float ir = v_InnerRadius * 0.5;
    float blur = fwidth(length(uv));

    vec4 col = v_FragColor;

    float alphaR = smoothstep(r, r - blur, length(uv));
    float alphaIR = smoothstep(ir - blur, ir, length(uv));
    float alpha = alphaR * alphaIR;
    alpha = clamp(alpha, 0.0, 1.0);

    if(alpha < 0.001) discard;

    fragColor = vec4(col.rgb, min(alpha, col.a));
    fragColor1 = v_FragId;
}
