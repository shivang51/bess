#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec2 v_TexCoord;
in flat int v_FragId;
in vec4 v_FragColor;

uniform float u_zoom;
uniform vec2 u_cameraOffset;
uniform vec4 u_gridMinorColor;
uniform vec4 u_gridMajorColor;
uniform vec4 u_axisXColor;
uniform vec4 u_axisYColor;
uniform vec2 u_resolution;

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

    float smallGrid = gridLine(fragCoord, smallSpacing, 1.0);
    float bigGrid = gridLine(fragCoord, bigSpacing, 2.0);

    float smallFade = clamp((u_zoom - 0.5) * 2.0, 0.0, 1.0);
    smallGrid *= smallFade;

    float intensity = max(smallGrid * 0.3, bigGrid * 0.8);

    if (intensity <= 0.0)
        discard;

    vec4 gridColor = vec4(0.0);

    if (smallGrid >= bigGrid) {
        gridColor = u_gridMajorColor;
    } else {
        gridColor = u_gridMinorColor;
    }

    float axisThicknessWorld = 1.0 / u_zoom;
    if (abs(fragCoord.x) < axisThicknessWorld)
        gridColor = u_axisYColor;
    if (abs(fragCoord.y) < axisThicknessWorld)
        gridColor = u_axisXColor;

    fragColor = vec4(gridColor.rgb, min(intensity, gridColor.a));
    fragColor1 = v_FragId;
}
