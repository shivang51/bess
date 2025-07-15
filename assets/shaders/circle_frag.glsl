#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec3 v_FragPos;
in vec2 v_TexCoord;
in vec4 v_FragColor;
in float v_InnerRadius;
in flat int v_TextureIndex;

uniform int u_SelectedObjId;

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
    fragColor1 = v_TextureIndex;
}
