using SkiaSharp;

namespace BessScene.SceneCore.Entities;

public class Shader
{
    private readonly SKRuntimeEffect _effect;
    private readonly SKRuntimeEffectUniforms? _uniforms;
    
    public Shader(string source)
    {
        _effect = SKRuntimeEffect.CreateShader(source, out var errors);
        if (errors != null)
        {
            throw new Exception($"Error creating shader: {errors}");
        }
    }
    
    
    public Shader(string source, Dictionary<string, dynamic> uniforms)
    {
        _effect = SKRuntimeEffect.CreateShader(source, out var errors);
        if (errors != null)
        {
            throw new Exception($"Error creating shader: {errors}");
        }

        _uniforms = new SKRuntimeEffectUniforms(_effect);

        foreach (var (key, value) in uniforms)
        {
            _uniforms.Add(key, value);
        }
    }
    
    public void SetUniform(string name, dynamic value)
    {
        if (_uniforms == null)
        {
            throw new Exception("No uniforms in this shader");
        }
        
        _uniforms.Add(name, value);
    }

    public SKShader Get => _effect.ToShader(_uniforms);
}