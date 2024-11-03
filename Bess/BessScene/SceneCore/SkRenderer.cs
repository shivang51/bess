using System.Numerics;
using SkiaSharp;

namespace BessScene.SceneCore;

public static class SkRenderer
{
    private static SKCanvas ColorCanvas { get; set;}
    
    private static SKCanvas IdCanvas { get; set;}

    public static void Begin(SKCanvas colorCanvas, SKCanvas idCanvas)
    {
        ColorCanvas = colorCanvas;
        IdCanvas = idCanvas;
    }

    public static void DrawRoundRect(SKPoint position, SKSize size, float radius, SKColor color, SKColor renderId)
    {
        var rect = new SKRect(position.X, position.Y, position.X + size.Width, position.Y + size.Height);
        var roundedRect = new SKRoundRect(rect , radius);
        
        using var paint = new SKPaint();
        paint.IsAntialias = true;
        paint.Color = color;
        ColorCanvas.DrawRoundRect(roundedRect, paint);
        
        using var idPaint = new SKPaint();
        idPaint.Color = renderId;
        IdCanvas.DrawRoundRect(roundedRect, idPaint);
    }
    
    public static void DrawRoundRect(SKPoint position, SKSize size, Vector4 radius, SKColor color, SKColor renderId)
    {
        var rect = new SKRect(position.X, position.Y, position.X + size.Width, position.Y + size.Height);
        var roundedRect = new SKRoundRect();
        
        roundedRect.SetRectRadii(rect , new[]
        {
            new SKPoint(radius.X, radius.X),
            new SKPoint(radius.Y, radius.Y),
            new SKPoint(radius.Z, radius.Z),
            new SKPoint(radius.W, radius.W),
        });
        
        using var paint = new SKPaint();
        paint.IsAntialias = true;
        paint.Color = color;
        ColorCanvas.DrawRoundRect(roundedRect, paint);
        
        using var idPaint = new SKPaint();
        idPaint.Color = renderId;
        IdCanvas.DrawRoundRect(roundedRect, idPaint);
    }

    public static void DrawRoundRect(SKPoint position, SKSize size, float radius, SKColor color, SKColor renderId,
        SKColor borderColor, float borderWidth)
    {
        var rect = new SKRect(position.X, position.Y, position.X + size.Width, position.Y + size.Height);
        var roundedRect = new SKRoundRect(rect, radius);

        using var paint = new SKPaint();
        paint.IsStroke = true;
        paint.IsAntialias = true;
        paint.Color = borderColor;
        paint.StrokeWidth = borderWidth;
        ColorCanvas.DrawRoundRect(roundedRect, paint);

        paint.IsAntialias = false;
        paint.Color = renderId;
        IdCanvas.DrawRoundRect(roundedRect, paint);
        
        DrawRoundRect(position, size, radius, color, renderId);
    }

    public static void DrawCircle(SKPoint position, float radius, SKColor color, SKColor renderId)
    {
        using var paint = new SKPaint();
        paint.IsAntialias = true;
        paint.Color = color;
        ColorCanvas.DrawCircle(position, radius, paint);
        
        using var idPaint = new SKPaint();
        idPaint.Color = renderId;
        IdCanvas.DrawCircle(position, radius, idPaint);
    }

    public static void DrawCircle(SKPoint position, float radius, SKColor color, SKColor renderId, SKColor borderColor, float borderWidth)
    {
        using var paint = new SKPaint();
        paint.IsStroke = true;
        paint.StrokeWidth = borderWidth;
        paint.IsAntialias = true;
        paint.Color = borderColor;
        ColorCanvas.DrawCircle(position, radius, paint);
        
        paint.IsAntialias = false;
        paint.Color = renderId;
        IdCanvas.DrawCircle(position, radius, paint);
        
        DrawCircle(position, radius, color, renderId);
    }
    
    public static void DrawText(string text, SKPoint position, SKColor color, float fontSize)
    {
        using var paint = new SKPaint();
        paint.TextSize = fontSize;
        paint.Color = color;
        paint.IsAntialias = true;
        position.Y += fontSize / 2 - paint.FontMetrics.Descent;
        ColorCanvas.DrawText(text, position, paint);
    }
}