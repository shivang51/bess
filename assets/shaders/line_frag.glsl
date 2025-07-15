#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec3 v_FragPos;
in vec2 v_TexCoord;
in vec4 v_FragColor;
in flat int v_FragId;

uniform int u_SelectedObjId;

void main(){
		vec2 uv = v_TexCoord;
		uv -= 0.5f;

		vec3 col = v_FragColor.rgb;
		float m = (1.f - abs(uv.y - 0.05)) * 1.1f;
		col *= m;

    fragColor = vec4(col, v_FragColor.a);
    fragColor1 = v_FragId;	
}
