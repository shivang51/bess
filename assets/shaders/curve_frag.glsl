#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec3 v_FragPos;
in vec2 v_TexCoord;
in vec4 v_FragColor;
in flat int v_FragId;

uniform int u_SelectedObjId;
uniform float u_zoom;

void main() {
    int id = u_SelectedObjId;
    vec2 uv = v_TexCoord - 0.5;
    vec4 col = v_FragColor;

    float smoothBlur = fwidth(length(uv));

		// float dashAlpha =  1.f;
		// if(false){
		// 	float dashPattern = fract(v_TexCoord.x * 10.0);
		// 	dashAlpha = smoothstep(0.5, 0.5 - fwidth(dashPattern), dashPattern);
		// }

    float alpha = smoothstep(0.5,  0.5 - smoothBlur, abs(uv.y));

		// alpha = min(alpha, dashAlpha);

    if(alpha == 0) discard;

    col.w = min(col.w, alpha);

    fragColor = col;
    fragColor1 = v_FragId;
}
