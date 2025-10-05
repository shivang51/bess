#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec3 v_FragPos;
in vec2 v_TexCoord;
in vec4 v_FragColor;
in flat int v_FragId;

uniform int u_SelectedObjId;

void main(){
    fragColor = v_FragColor;
    fragColor1 = v_FragId;	
}
