#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec3 v_FragPos;
in vec2 v_TexCoord;
in vec3 v_FragColor;
in flat int v_TextureIndex;

uniform int u_SelectedObjId;


float roundedCorner(vec2 position, float radius) {
    float dist = length(position);
    return smoothstep(radius - 0.5, radius + 0.5, radius - dist);
}

void main() {
    vec2 coords = v_TexCoord;
    vec2 u_dimensions = vec2(0.3f, 0.25f);
    float u_radius = 0.1f;

    if (length(coords - vec2(0)) < u_radius ||
        length(coords - vec2(0, u_dimensions.y)) < u_radius ||
        length(coords - vec2(u_dimensions.x, 0)) < u_radius ||
        length(coords - u_dimensions) < u_radius) {
        discard;
    }
    // Apply color with alpha
    fragColor = vec4(v_FragColor, 1.f);
    fragColor1 = v_TextureIndex;
}
