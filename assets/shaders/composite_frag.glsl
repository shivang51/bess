#version 460 core

layout(location = 0) out vec4 fragColor;

in vec2 v_TexCoord;

uniform sampler2D uBaseColorTex;
uniform sampler2D uShadowTex;

void main() {
    vec2 texCoord = vec2(v_TexCoord.x, 1.f - v_TexCoord.y);

    vec4 col = texture(uBaseColorTex, texCoord);
    vec4 shadow = texture(uShadowTex, texCoord);
    vec4 shadow1 = texture(uShadowTex, texCoord + 0.1f);

		float shadowValue =  step(0.8f, shadow.w);

		vec4 final = col;
		if(shadow.r > 0.001f) {
			final = mix(col, vec4(col.rgb + (shadow.r * (1.f - col.w) * 0.025), 0.5f), shadowValue);
		}
		  // vec4 final = mix(col, vec4(shadow.rgb * 0.2, 0.1f), shadowValue);
    fragColor = vec4(final);
}
