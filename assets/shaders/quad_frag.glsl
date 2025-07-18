#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec2 v_Size;
in vec2 v_TexCoord;
in vec4 v_FragColor;
in vec4 v_BorderRadius; // ordering: top-left, top-right, bottom-right, bottom-left
in vec4 v_BorderSize;   // ordering: top, right, bottom, left
in vec4 v_BorderColor;
in flat int v_FragId;
in flat int v_IsMica;
in flat int v_TexSlotIdx;

uniform sampler2D u_Textures[32];
uniform int u_SelectedObjId;
uniform float u_zoom;

float rand(vec2 uv) {
    return fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
}

float sdRR(vec2 p, vec2 b, vec4 r) {
    float rc = p.x >= 0.0 && p.y >= 0.0 ? r.y :  // top-right quadrant
               p.x <  0.0 && p.y >= 0.0 ? r.x :  // top-left quadrant
               p.x >= 0.0 && p.y <  0.0 ? r.z : r.w; // bottom-right (else bottom-left)
    vec2 d = abs(p) - (b - vec2(rc));
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - rc;
}

float minVec2(vec2 v) {
    return (v.x <= v.y) ? v.x : v.y;
}

void main(){
    bool isMica = (v_IsMica == 1);
    vec2 fc = v_TexCoord;
    vec4 bgColor = v_FragColor;
    vec2 iResolution = v_Size;

    vec4 texColor = vec4(1.f);
    if(v_TexSlotIdx > 0){
        texColor = texture(u_Textures[v_TexSlotIdx], v_TexCoord);
    }
    
    vec2 uv = fc - 0.5;
    uv.x *= iResolution.x / iResolution.y;
    
    vec2 oC = vec2(0.0);
    vec2 oH = vec2((iResolution.x / iResolution.y) * 0.5, 0.5);
    float aa = fwidth(length(uv));
    
    vec4 oR = v_BorderRadius;
    oR /= iResolution.y;
    
    vec4 borderSize = v_BorderSize / iResolution.y;
    float bT = borderSize.x;
    float bR = borderSize.y;
    float bB = borderSize.z;
    float bL = borderSize.w;
    
    float dO = sdRR(uv - oC, oH, oR);
    
    float L = -oH.x, R = oH.x, B = -oH.y, T = oH.y;
    float iL = L + bL, iR = R - bR, iB = B + bB, iT = T - bT;
    vec2 iC = vec2((iL + iR) * 0.5, (iB + iT) * 0.5);
    vec2 iH = vec2((iR - iL) * 0.5, (iT - iB) * 0.5);
    
    float iTL = max(0.0, oR.x - min(bT, bL));
    float iTR = max(0.0, oR.y - min(bT, bR));
    float iBR = max(0.0, oR.z - min(bB, bR));
    float iBL = max(0.0, oR.w - min(bB, bL));
    vec4 iRads = vec4(iTL, iTR, iBR, iBL);
    
    float dI = sdRR(uv - iC, iH, iRads);
    
    float mO = smoothstep(aa, -aa, dO);
    float mI = smoothstep(aa, -aa, dI);

    if(mO < 0.01) discard;
    
    bgColor =  mix(v_BorderColor, bgColor, mI);
    bgColor.a *= mO;

    if(isMica){
        bgColor.rgb *= length((fc + vec2(-0.35f, -0.5f)));
    }

    bgColor *= texColor;

    fragColor = bgColor;
    fragColor1 = v_FragId;
}

