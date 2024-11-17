using System.Numerics;
using SkiaSharp;

namespace BessScene.SceneCore;

public class Transform
{
    public Vector2 Position { get; set; }
    
    public Vector2 Scale { get; set; }
    
    public float Rotation { get; set; }
    
    public Transform(Vector2 position, Vector2 scale, float rotation)
    {
        Position = position;
        Scale = scale;
        Rotation = rotation;
    }
    
    public Transform(Vector2 position, Vector2 scale)
    {
        Position = position;
        Scale = scale;
        Rotation = 0;
    }
    
    public static Vector2 ScaleAndTranslate(Vector2 point, Vector2 scale, Vector2 translate)
    {
        return new Vector2(point.X * scale.X + translate.X, point.Y * scale.Y + translate.Y);
    }
}