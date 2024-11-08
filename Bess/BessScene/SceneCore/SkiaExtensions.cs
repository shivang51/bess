using System.Numerics;
using SkiaSharp;

namespace BessScene.SceneCore;

public static class SkiaExtensions
{
    public static SKColor Multiply(this SKColor color, float value)
    {
        var r = color.Red * value;
        var g = color.Green * value;
        var b = color.Blue * value;
        var a = color.Alpha * value;
        
        return new SKColor((byte)r, (byte)g, (byte)b, (byte)a);
    }
    
    public static SKPoint ToSkPoint(this Vector2 vector) => new((float)vector.X, (float)vector.Y);
    
    private static readonly Random _random = new();
    public static SKColor GenerateRandomColor()
    {
        byte red = (byte)_random.Next(256);
        byte green = (byte)_random.Next(256);
        byte blue = (byte)_random.Next(256);
        byte alpha = 255; // Set to 255 for full opacity, or randomize if you want transparency

        return new SKColor(red, green, blue, alpha);
    }
}