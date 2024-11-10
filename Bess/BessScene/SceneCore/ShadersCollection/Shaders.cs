using SkiaSharp;

namespace BessScene.SceneCore.State.ShadersCollection;

public static class Shaders
{
    public static Shader GridShader { get; private set;}

    static Shaders()
    {
        var uniforms = new Dictionary<string, object>
        {
            {"zoom", CameraController.DefaultZoom},
            {"cameraOffset", new SKPoint(0, 0)},
            {"gap", 20.0f},
            {"dotSize", 1.5f}
        };
        GridShader = new Shader(ShadersSources.GridShaderSrc, uniforms);
    }
}