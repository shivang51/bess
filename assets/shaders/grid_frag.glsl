#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec2 v_TexCoord;
in flat int v_FragId;
in vec4 v_FragColor;

const float grid_intensity = 0.5f;
const float dotRadius = 2.f;

uniform float u_zoom;
uniform vec2 u_cameraOffset;
uniform vec2 u_resolution;

void main() {
    vec2 fragCoord = ((gl_FragCoord.xy - u_resolution * 0.5) / u_zoom) - u_cameraOffset;
    float scale = u_zoom;

    float gap = 10.f * scale;
    vec2 gridPos = mod(fragCoord, gap);

    vec2 dotCenter = vec2(gap / 2.0);

    float dist = distance(gridPos, dotCenter);
    float a = smoothstep(dotRadius, dotRadius - 1.f, dist);
    if (a == 0.f) discard;
    vec4 col = v_FragColor * a;

    fragColor = col;
    fragColor1 = v_FragId;
}
