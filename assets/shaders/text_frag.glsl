#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec2 v_TexCoord;
in vec4 v_FragColor;
in flat int v_FragId;
in flat int v_TexSlotIdx;

uniform sampler2D u_Textures[32];
uniform int u_SelectedObjId;
uniform float u_zoom;

float screenPxRange() {
	const float pxRange = 4.0; // set to distance field's pixel range
    vec2 unitRange = vec2(pxRange)/vec2(textureSize(u_Textures[v_TexSlotIdx], 0));
    vec2 screenTexSize = vec2(1.0)/fwidth(v_TexCoord);
    return max(0.5*dot(unitRange, screenTexSize), 1.0);
}

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}


void main(){
    vec4 bgColor = v_FragColor;
    if(v_TexSlotIdx == 0){
        fragColor = bgColor;
        fragColor1 = v_FragId;
        return;
    }

    vec3 msd = texture(u_Textures[v_TexSlotIdx], v_TexCoord).rgb;
    float sd = median(msd.r, msd.g, msd.b);
    float screenPxDistance = screenPxRange()*(sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);
    bgColor = mix(vec4(0.0), bgColor, opacity);
    if(bgColor.a <= 0.001f) discard;

    fragColor = bgColor;
    fragColor1 = v_FragId;
}