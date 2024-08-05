float sdBox(in vec2 p, in vec2 b)
{
    vec2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float rect(vec2 pos, vec2 size) {
    float d = 1.f - sdBox(pos, size);
    d = smoothstep(1.f - 0.002f, 1.f, d);
    return d;
}

vec3 getBlur(vec2 pos, vec2 offset, vec3 shade)
{
    return shade + texture(iChannel1, offset * 5.0).xyz * 0.07;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = (fragCoord - iResolution.xy * 0.5) / iResolution.y;
    vec2 mouse = (iMouse.xy - iResolution.xy * 0.5) / iResolution.y;

    vec2 offset = uv - mouse;

    vec3 blur = getBlur(uv, offset, vec3(.15f));
    vec3 col = blur;

    float sq = rect(offset, vec2(0.25f, 0.15f));
    col = mix(vec3(0.1f), col, sq);
    col = mix(vec3(0.1f), col, 0.5);

    fragColor = vec4(col, 1.f);
}

