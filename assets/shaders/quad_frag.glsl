#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec3 v_FragPos;
in vec2 v_TexCoord;
in vec3 v_FragColor;
in flat int v_TextureIndex;

uniform int u_SelectedObjId;

void main() {

    vec3 c = v_FragColor;


    fragColor = vec4(c, 1.f);

    fragColor1 = v_TextureIndex;
}
