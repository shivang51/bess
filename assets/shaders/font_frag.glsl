#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec3 v_FragPos;
in vec2 v_TexCoord;
in vec3 v_FragColor;
in flat int v_TextureIndex;

uniform sampler2D text;
uniform int u_SelectedObjId;
uniform vec3 textColor;

void main(){
    float a = texture(text, v_TexCoord).r;
    if(a == 0.f) discard;
    vec4 sampled = vec4(1.0, 1.0, 1.0, a);
    fragColor = vec4(textColor, 1.0) * sampled;
    fragColor1 = v_TextureIndex;	
}