#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;
layout(location = 2) out vec4 fragColor2;

in vec3 v_FragPos;
in vec2 v_TexCoord;
in vec4 v_FragColor;
in flat int v_TextureIndex;

uniform sampler2D tex;
uniform int u_SelectedObjId;
uniform vec4 textColor;

void main(){
    float a = texture(tex, v_TexCoord).r;
    if(a == 0.f) discard;

    vec4 sampled = textColor;
    sampled.w = min(sampled.w, a);

    fragColor = sampled;
    fragColor1 = v_TextureIndex;
		fragColor2 = vec4(0.f);
}
