#version 460

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

layout(location = 0) in vec3 v_FragPos;
layout(location = 1) in vec4 v_FragColor;
layout(location = 2) in vec2 v_TexCoord;
layout(location = 3) in flat int v_FragId;
layout(location = 4) in flat int v_TexSlotIdx;

layout(binding = 1) uniform ZoomUniforms {
    float u_zoom;
};

void main() {
    vec2 uv = v_TexCoord - 0.5;
    vec4 col = v_FragColor;

    float smoothBlur = fwidth(length(uv.y));

    float alpha = smoothstep(0.5 + smoothBlur, 0.5 - smoothBlur, abs(uv.y));

    if (alpha == 0) discard;
    col *= alpha;

    fragColor = col;
    fragColor1 = v_FragId;
}
