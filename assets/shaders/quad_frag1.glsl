#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec3 v_FragPos;
in vec2 v_TexCoord;
in vec3 v_FragColor;
in flat int v_FragId;
in vec4 v_BorderRadius;
in float v_AR;

uniform int u_SelectedObjId;

float roundedBoxSDF(vec2 CenterPosition, vec2 Size, float Radius) {
    return length(max(abs(CenterPosition) - Size + Radius, 0.0)) - Radius;
}

void main() {
    float ar = v_AR;
    vec4 radi = v_BorderRadius;
    vec2 uv = v_TexCoord;

    vec2 size = vec2(1.f);

    float radius = 0.f;
    if (uv.y > 0.5) {
        radius = (uv.x < 0.5) ? radi[0] : radi[1];
    } else {
        radius = (uv.x < 0.5) ? radi[3] : radi[2];
    }

    uv.x *= ar;

    size /= 2.f;
    size.x *= ar;

    float dis = roundedBoxSDF(uv.xy - size, size, radius);

    float alpha = 1.0f - smoothstep(0.0f, 0.02, dis);

    if (alpha == 0.f) discard;

    fragColor = vec4(v_FragColor, alpha);
    fragColor1 = v_FragId;
}
