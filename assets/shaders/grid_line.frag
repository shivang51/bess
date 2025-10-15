#version 460

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int pickingId;

layout(location = 1) in vec2 v_TexCoord;
layout(location = 2) in flat int v_FragId;
layout(location = 4) in vec4 v_FragColor;

layout(binding = 1) uniform GridUniforms {
    float u_zoom;
    vec2 u_cameraOffset;
    vec4 u_gridMinorColor;
    vec4 u_gridMajorColor;
    vec4 u_axisXColor;
    vec4 u_axisYColor;
    vec2 u_resolution;
};

const float smallSpacing = 10.0;
const float bigSpacing = 100.0;

float gridLine(vec2 worldPos, float spacing, float thicknessPx) {
    vec2 dist = abs(mod(worldPos, spacing));
    dist = min(dist, spacing - dist);

    float thicknessWorld = thicknessPx / u_zoom;

    float d = min(dist.x, dist.y);
    return smoothstep(thicknessWorld, 0.0, d);
}

void main() {
    vec2 fragCoord = ((gl_FragCoord.xy - u_resolution * 0.5) / u_zoom) - u_cameraOffset;

    float smallGrid = gridLine(fragCoord, smallSpacing, 1.f);
    float bigGrid = gridLine(fragCoord, bigSpacing, 1.5f);

    float smallFade = clamp((u_zoom - 0.5) * 2.0, 0.0, 1.0);
    smallGrid *= smallFade;

    float intensity = max(smallGrid * 0.3, bigGrid * 0.6);

    if (intensity <= 0.0001)
        discard;

    vec4 gridColor = vec4(1.0);

    if (smallGrid >= bigGrid) {
        gridColor *= u_gridMinorColor;
    } else {
        gridColor *= u_gridMajorColor;
    }

    float axisThicknessWorld = 1.5 / u_zoom;

    if (abs(fragCoord.x) < axisThicknessWorld)
        gridColor = u_axisYColor;
    if (abs(fragCoord.y) < axisThicknessWorld)
        gridColor = u_axisXColor;

    gridColor.rgb *= gridColor.a;

    fragColor = gridColor;
    pickingId = v_FragId;
}
