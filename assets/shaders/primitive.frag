#version 460

layout(location = 0) out vec4 fragColor;
layout(location = 1) out uvec2 pickingId;

layout(location = 0) in vec4 v_FragColor;
layout(location = 1) in vec2 v_TexCoord;
layout(location = 2) in vec2 v_LocalCoord;
layout(location = 3) in vec4 v_BorderRadius;
layout(location = 4) in vec4 v_BorderSize;
layout(location = 5) in vec4 v_BorderColor;
layout(location = 6) in vec2 v_Size;
layout(location = 7) in vec4 v_PrimitiveData;
layout(location = 8) in flat uvec2 v_FragId;
layout(location = 9) in flat int v_IsMica;
layout(location = 10) in flat int v_TexSlotIdx;
layout(location = 11) in flat int v_PrimitiveType;

layout(set = 1, binding = 2) uniform sampler2D uTextures[32];

const int PRIMITIVE_TYPE_QUAD = 0;
const int PRIMITIVE_TYPE_CIRCLE = 1;

float sdRoundedRect(vec2 p, vec2 halfExtents, vec4 radii) {
    float radius = p.x >= 0.0 && p.y >= 0.0 ? radii.y :
        p.x < 0.0 && p.y >= 0.0 ? radii.x :
        p.x >= 0.0 && p.y < 0.0 ? radii.z : radii.w;
    vec2 d = abs(p) - (halfExtents - vec2(radius));
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - radius;
}

vec4 shadeQuad() {
    float safeHeight = max(abs(v_Size.y), 0.0001);
    vec2 quadCoord = v_LocalCoord;
    quadCoord.x *= v_Size.x / safeHeight;

    vec2 outerHalf = vec2((v_Size.x / safeHeight) * 0.5, 0.5);
    float aa = fwidth(length(quadCoord));

    vec4 outerRadii = v_BorderRadius / safeHeight;
    vec4 borderSize = v_BorderSize / safeHeight;

    float outerDistance = sdRoundedRect(quadCoord, outerHalf, outerRadii);

    float left = -outerHalf.x;
    float right = outerHalf.x;
    float bottom = -outerHalf.y;
    float top = outerHalf.y;
    float innerLeft = left + borderSize.w;
    float innerRight = right - borderSize.y;
    float innerBottom = bottom + borderSize.z;
    float innerTop = top - borderSize.x;
    vec2 innerCenter = vec2((innerLeft + innerRight) * 0.5, (innerBottom + innerTop) * 0.5);
    vec2 innerHalf = vec2((innerRight - innerLeft) * 0.5, (innerTop - innerBottom) * 0.5);

    vec4 innerRadii = vec4(
        max(0.0, outerRadii.x - min(borderSize.x, borderSize.w)),
        max(0.0, outerRadii.y - min(borderSize.x, borderSize.y)),
        max(0.0, outerRadii.z - min(borderSize.z, borderSize.y)),
        max(0.0, outerRadii.w - min(borderSize.z, borderSize.w))
    );

    float innerDistance = sdRoundedRect(quadCoord - innerCenter, innerHalf, innerRadii);
    float outerMask = smoothstep(aa, -aa, outerDistance);
    float innerMask = smoothstep(aa, -aa, innerDistance);

    if (outerMask < 0.001) {
        discard;
    }

    vec4 baseColor = v_FragColor;
    if (v_TexSlotIdx != 0) {
        baseColor *= texture(uTextures[v_TexSlotIdx], v_TexCoord);
    }

    if (v_IsMica == 1) {
        vec3 tintColor = mix(vec3(0.05), v_FragColor.rgb, 0.15);
        float innerGlow = smoothstep(-0.5, 0.2, innerDistance);
        float topLight = smoothstep(0.5, 0.0, v_TexCoord.y) * 0.25;
        topLight *= smoothstep(1.0, 0.4, v_TexCoord.x);

        vec3 glassRgb = tintColor;
        glassRgb += innerGlow * 0.1;
        glassRgb += topLight;

        baseColor.rgb *= glassRgb;
        baseColor.a = 0.92;
    }

    vec4 color = mix(v_BorderColor, baseColor, innerMask);
    color.a *= outerMask;
    return color;
}

vec4 shadeCircle() {
    float outerRadius = max(v_PrimitiveData.x, 0.0);
    float innerRadius = clamp(v_PrimitiveData.y, 0.0, outerRadius);
    float innerRatio = outerRadius > 0.0 ? innerRadius / outerRadius : 0.0;

    float dist = length(v_LocalCoord);
    float aa = fwidth(dist) * 0.5;
    float outerMask = 1.0 - smoothstep(0.5 - aa, 0.5 + aa, dist);
    float innerMask = smoothstep((innerRatio * 0.5) - aa, (innerRatio * 0.5) + aa, dist);
    float mask = outerMask * innerMask;

    if (mask < 0.001) {
        discard;
    }

    vec4 color = v_FragColor;
    color.a *= mask;
    return color;
}

void main() {
    vec4 color;

    if (v_PrimitiveType == PRIMITIVE_TYPE_QUAD) {
        color = shadeQuad();
    } else if (v_PrimitiveType == PRIMITIVE_TYPE_CIRCLE) {
        color = shadeCircle();
    } else {
        discard;
    }

    if (color.a < 0.001) {
        discard;
    }

    color.rgb *= color.a;
    fragColor = color;
    pickingId = v_FragId;
}
