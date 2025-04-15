#version 460 core

layout(location = 0) out vec4 fragColor;

in vec2 v_TexCoord;

uniform sampler2D tex;

void main() {
		vec2 texCoord = vec2(v_TexCoord.x, 1.f - v_TexCoord.y);
    vec4 col = texture(tex, texCoord);
		if(col.r != 1.f) discard;
			
		float d = col.g;
		vec4 bgColor = vec4(vec3(d * 0.2f), 1.f - d); 
		
    fragColor = bgColor;
}
