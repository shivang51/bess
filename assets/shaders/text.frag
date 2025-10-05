#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec2 v_TexCoord;
in vec4 v_FragColor;
in flat int v_FragId;
in flat int v_TexSlotIdx;

uniform sampler2D u_Textures[32];
uniform float u_pxRange;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    if (v_TexSlotIdx == 0) {
        fragColor = v_FragColor;
        fragColor1 = v_FragId;
        return;
    }

    vec4 msd = texture(u_Textures[v_TexSlotIdx], v_TexCoord);
		float sdMsdf = median(msd.r, msd.g, msd.b);
		float sdSdf  = msd.a;

    float scale = length(fwidth(v_TexCoord) * textureSize(u_Textures[v_TexSlotIdx], 0));

		float blend = smoothstep(8.f, 16.f, scale);
		float dist = mix(sdSdf, sdMsdf, blend);

    float screenDist = (dist - 0.5) * u_pxRange / scale;

		float opacity = smoothstep(-0.5, 0.5, screenDist);

    vec4 finalColor = vec4(v_FragColor.rgb, v_FragColor.a * opacity);

    if (finalColor.a < 0.001) {
        discard;
    }

    fragColor = finalColor;
    fragColor1 = v_FragId;
}
