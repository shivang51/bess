#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec3 v_FragPos;
in vec2 v_TexCoord;
in flat int v_FragId;
in float v_AR;

void main() {
    float ar = v_AR;
    vec2 uv = v_TexCoord;

    float div = 0.1f, subDiv = div / 2.f;
    float thickness = 0.001;

    float m = max(mod(uv.x, div), mod(uv.y, div));
    float alpha = step(div - thickness, m);
    vec3 col = vec3(.5f * alpha);

    if (alpha == 0.f) {
        float m = max(mod(uv.x, subDiv), mod(uv.y, subDiv));
        alpha = step(subDiv - thickness, m);
        col = vec3(0.2f);
    }

    if (alpha == 0.f) discard;

    //fragColor = vec4(vec3(uv.x), 1.f);
    fragColor = vec4(col, alpha);
    fragColor1 = v_FragId;
}
