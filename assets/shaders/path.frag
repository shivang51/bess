#version 460

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

layout(location = 0) in vec3 v_FragPos;
layout(location = 1) in vec4 v_FragColor;
layout(location = 2) in vec2 v_TexCoord;
layout(location = 3) in flat int v_FragId;
layout(location = 4) in flat int v_TexSlotIdx;
layout(location = 5) noperspective in vec3 v_Barycentric;

layout(binding = 1) uniform ZoomUniforms {
    float u_zoom;
};

void main() {
    vec2 dx = dFdx(v_FragPos.xy);
    vec2 dy = dFdy(v_FragPos.xy);

    float pixelSize = length(dx) + length(dy);

    pixelSize *= 1.0 / max(u_zoom, 1e-6);

    float dist = 1.0 - min(v_Barycentric.x, min(v_Barycentric.y, v_Barycentric.z));
    float w = max(fwidth(dist), 1e-6);
    float coverage = smoothstep(0.0, w * 1.5, dist);
    coverage = max(coverage, 0.95);

    vec4 color = v_FragColor;
    color.a *= coverage;

    if (color.a < 0.0001f) discard;
    color.rgb *= color.a;

    fragColor = color;
    fragColor1 = v_FragId;
}
