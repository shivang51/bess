#version 460 core

layout(location = 0) out vec4 fragColor;

in vec2 v_TexCoord;

uniform sampler2D tex;

void main() {
		vec2 texCoord = vec2(v_TexCoord.x, 1.f - v_TexCoord.y);
    vec4 col = texture(tex, texCoord);
			
		vec4 bgColor = vec4(0.f);
		if(col.r == 1.f){
			bgColor = vec4(vec3(col.g), 0.7f); 
		}
    fragColor = bgColor;
}
