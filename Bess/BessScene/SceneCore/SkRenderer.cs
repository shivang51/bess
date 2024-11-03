using System.Numerics;
using SkiaSharp;

namespace BessScene.SceneCore;

public static class SkRenderer
{
    private const string FontFilePath = "Assets/fonts/Roboto/Roboto-Regular.ttf";

    private static SKCanvas ColorCanvas { get; set; } = null!;
    private static SKCanvas IdCanvas { get; set; } = null!;

    private static readonly SKTypeface? FontTypeFace;

    static SkRenderer()
    {
        FontTypeFace = SKTypeface.FromFile(FontFilePath);
        if (FontTypeFace == null)
        {
            Console.Error.WriteLine($"Failed to load font at {Directory.GetCurrentDirectory()}{Path.DirectorySeparatorChar}{FontFilePath}");
        }
    }

    public static void Begin(SKCanvas colorCanvas, SKCanvas idCanvas)
    {
        ColorCanvas = colorCanvas;
        IdCanvas = idCanvas;
    }
    
    public static void DrawMicaRoundRect(SKPoint position, SKSize size, Vector4 radius, SKColor? renderId = null)
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
        
        DrawMicaRRect(rect, roundedRect);
        if(renderId != null) DrawRRectId(rect, roundedRect, (SKColor)renderId!);
    }
    
    public static void DrawMicaRoundRect(SKPoint position, SKSize size, float radius, SKColor? renderId = null)
    {
        var rect = new SKRect(position.X, position.Y, position.X + size.Width, position.Y + size.Height);
        var roundedRect = new SKRoundRect(rect , radius);
        
        DrawMicaRRect(rect, roundedRect);
        if(renderId != null) DrawRRectId(rect, roundedRect, (SKColor)renderId!);
    }
    
    public static void DrawAcrylicQuad(SKPoint position, SKSize size, float radius, SKColor color, SKColor renderId)
    {
        var rect = new SKRect(position.X, position.Y, position.X + size.Width, position.Y + size.Height);
        var roundedRect = new SKRoundRect(rect , radius);
        
        // Background blur paint
        using (var blurPaint = new SKPaint())
        {
            blurPaint.ImageFilter = SKImageFilter.CreateBlur(10f, 10f);
            blurPaint.IsAntialias = true;
            ColorCanvas.SaveLayer(blurPaint);
            ColorCanvas.DrawRoundRect(roundedRect, blurPaint);
            ColorCanvas.Restore();
        }

        // Semi-transparent overlay paint
        using (var overlayPaint = new SKPaint())
        {
            overlayPaint.Color = new SKColor(255, 255, 255, 30); // Adjust alpha for translucency
            overlayPaint.BlendMode = SKBlendMode.SrcOver;
            overlayPaint.IsAntialias = true;
            // Draw the acrylic overlay
            ColorCanvas.DrawRoundRect(roundedRect, overlayPaint);
        }
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
        paint.Typeface = FontTypeFace;
        paint.TextSize = fontSize;
        paint.Color = color;
        paint.IsAntialias = true;
        position.Y += fontSize / 2 - paint.FontMetrics.Descent;
        ColorCanvas.DrawText(text, position, paint);
    }
    
    public static float CalculateTextWidth(string text, float fontSize)
    {
        using var paint = new SKPaint();
        paint.Typeface = FontTypeFace;
        paint.TextSize = fontSize;
        return paint.MeasureText(text);
    }

    private static void DrawRRectId(SKRect rect, SKRoundRect roundedRect, SKColor id)
    {
        using var idPaint = new SKPaint();
        idPaint.Color = id;
        IdCanvas.DrawRoundRect(roundedRect, idPaint);
    }
    
    private static void DrawMicaRRect(SKRect rect, SKRoundRect roundedRect)
    {
        // Step 1: Background gradient
        using (var gradientPaint = new SKPaint())
        {
            gradientPaint.IsAntialias = true;
            // Define gradient colors and positions to add depth
            gradientPaint.Shader = SKShader.CreateLinearGradient(
                new SKPoint(rect.Left, rect.Top),
                new SKPoint(rect.Right, rect.Bottom),
                new[] { new SKColor(30, 30, 30, 100), new SKColor(60, 60, 60, 150) },
                new float[] { 0.0f, 1.0f },
                SKShaderTileMode.Clamp
            );
        
            ColorCanvas.DrawRoundRect(roundedRect, gradientPaint);
        }

        // Step 2: Apply a blur to simulate frosted glass effect on the background
        using (var blurPaint = new SKPaint())
        {
            blurPaint.ImageFilter = SKImageFilter.CreateBlur(8f, 8f); // Adjust blur radius as needed
            blurPaint.IsAntialias = true;
            
            ColorCanvas.SaveLayer(blurPaint);
            ColorCanvas.DrawRoundRect(roundedRect, blurPaint);
            ColorCanvas.Restore();
        }

        // Step 3: Overlay with semi-transparent tint to finalize the mica effect
        using (var overlayPaint = new SKPaint())
        {
            overlayPaint.Color = new SKColor(255, 255, 255, 40); // Adjust alpha for subtle tint
            overlayPaint.BlendMode = SKBlendMode.SrcOver;
            overlayPaint.IsAntialias = true;
            
            // Draw the overlay tint for the Mica look
            ColorCanvas.DrawRoundRect(roundedRect, overlayPaint);
        }
    }
}