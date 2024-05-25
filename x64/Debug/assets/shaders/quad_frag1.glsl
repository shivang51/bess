#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec3 v_FragPos;
in vec2 v_TexCoord;
in vec3 v_FragColor;
in flat int v_TextureIndex;

uniform int u_SelectedObjId;

float sdRoundBox(in vec2 p, in vec2 b, in vec4 r)
{
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x = (p.y > 0.0) ? r.x : r.y;
    vec2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}

void main()
{
    //	vec2 p = (fragCoord-iResolution.xy)/iResolution.y ;
    //  p += 1.;

    vec2 p = v_TexCoord;

    p.y *= -1;

    //p -= 0.5;

    vec2 si = vec2(.01f, .01f);
    vec4 ra = vec4(.1);

    float d = sdRoundBox(p, si, ra);

    //  vec3 col = (d>0.0) ? vec3(0.0) : vec3(0.65,0.85,1.0);
    vec3 col = v_FragColor;
    //col = mix(col, vec3(1.0), smoothstep(0.0, 0.002, abs(d)));

    fragColor = vec4(col, 1.f - d);
    fragColor1 = v_TextureIndex;
}
