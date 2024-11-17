namespace BessScene.SceneCore.ShadersCollection;

public static class ShadersSources
{
    public static string GridShaderSrc = @"
uniform float zoom;
uniform vec2 cameraOffset;
uniform float gap;
uniform float dotSize;
uniform vec4 color; 

half4 main(vec2 fragCoord) {
    vec2 gridPos = mod((fragCoord / zoom) - cameraOffset, gap);
    float dist = distance(gridPos, vec2(gap / 2.0));
    float alpha = smoothstep(dotSize, dotSize - 1.0, dist);
    if(alpha == 0.0){
        return half4(0.0, 0.0, 0.0, 0.0);
    }
    return color * alpha;
}";

}