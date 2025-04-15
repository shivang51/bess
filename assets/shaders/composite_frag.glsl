#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec2 v_TexCoord;

uniform sampler2D uBaseColorTex;
uniform sampler2D uShadowTex;

void main() {
		vec2 texCoord = vec2(v_TexCoord.x, 1.f - v_TexCoord.y);

    vec4 col = texture(uBaseColorTex, texCoord);
    vec4 shadow = texture(uShadowTex, texCoord);

		vec4 bgColor = vec4(0);
		bgColor = mix(shadow, col, shadow.w); 

    fragColor = vec4(1.f);
		fragColor1 = 0;
}
