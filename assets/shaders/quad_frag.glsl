#version 460 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int fragColor1;

in vec2 v_Size;
in vec2 v_TexCoord;
in vec4 v_FragColor;
in vec4 v_BorderRadius;
in vec4 v_BorderSize;
in vec4 v_BorderColor;
in flat int v_FragId;
in flat int v_IsMica;

uniform int u_SelectedObjId;
uniform float u_zoom;

float rand(vec2 uv) {
    return fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
}

float sdRR(vec2 p, vec2 b, vec4 r){
    float rc = p.x>=0.0 && p.y>=0.0 ? r.y :
               p.x<0.0  && p.y>=0.0 ? r.z :
               p.x>=0.0 && p.y<0.0  ? r.x : r.w;
    vec2 d = abs(p) - (b - vec2(rc));
    return length(max(d,0.0)) + min(max(d.x,d.y),0.0) - rc;
}
float minVec2(vec2 v){
    return (v.x <= v.y) ? v.x : v.y;  
}

void main(){
    bool isMica = v_IsMica == 1;
    vec2 fc = v_TexCoord;
    vec4 bgColor = v_FragColor;
    vec2 iResolution = v_Size;
    vec2 uv= fc - 0.5f;
    uv.x *= iResolution.x / iResolution.y;
    vec2 oC = vec2(0.0), oH = vec2((iResolution.x / iResolution.y) * 0.5 , 0.5);
    float aa = fwidth(length(uv));
    
    vec4 oR = v_BorderRadius;
    oR /= minVec2(iResolution);
    vec4 borderSize = v_BorderSize / minVec2(iResolution);
    float bL = borderSize.w, bR = borderSize.y, bT = borderSize.x, bB = borderSize.w;
    
    float dO = sdRR(uv-oC, oH, oR);
    
    float L = -oH.x, R = oH.x, B = -oH.y, T = oH.y;
    float iL = L + bL, iR = R - bR, iB = B + bB, iT = T - bT;
    vec2 iC = vec2((iL+iR)*0.5, (iB+iT)*0.5),
         iH = vec2((iR-iL)*0.5, (iT-iB)*0.5);
    float iBR = max(0.0, oR.x - min(bR,bB));
    float iTR = max(0.0, oR.y - min(bR,bT));
    float iTL = max(0.0, oR.z - min(bL,bT));
    float iBL = max(0.0, oR.w - min(bL,bB));
    vec4 iRads = vec4(iBR, iTR, iTL, iBL);
    
    float dI = sdRR(uv-iC, iH, iRads);
    
    float mO = smoothstep(aa,-aa,dO),
          mI = smoothstep(aa,-aa,dI);

    if (mO == 0.f) discard;
    
    vec4 col = mix(v_BorderColor, bgColor, mI);
    col.a = min(mO, col.a);
    if(isMica){
        float noise = rand(uv * 80.f) * 0.1f;
        vec4 tintColor = vec4(0.5f, 0.5f, 0.8f, 0.25f);
        vec4 micaColor = mix(vec4(vec3(0.1f, 0.1f, 0.5f), 0.25f), tintColor, length(v_TexCoord.x));
        micaColor.rgb += noise * 0.8f; // Add subtle grain
        bgColor.rgb *= micaColor.rgb;
    }
    fragColor = col;
    fragColor1 = v_FragId;
}

