#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec3 v_FragPos;
in vec2 v_TexCoord;
in vec4 v_FragColor;
in flat int v_TextureIndex;

uniform int u_SelectedObjId;

void main() {
    int id = u_SelectedObjId;

    vec2 uv = (v_TexCoord - 0.5);
    float r = 0.5f;
    float blur = fwidth(length(uv));

    vec4 col = v_FragColor;

    float alpha = smoothstep(r, r - blur, length(uv));
    if (alpha < 0.001f) discard;

		float m = (1.f - abs(uv.y - 0.05)) * 1.1f;
		col *= m;

    fragColor = vec4(col.rgb, alpha);
    fragColor1 = v_TextureIndex;
}
