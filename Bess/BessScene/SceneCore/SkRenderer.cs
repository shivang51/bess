using System.Numerics;
using SkiaSharp;

namespace BessScene.SceneCore.State.ShadersCollection;

public static class SkRenderer
{
    private const string FontFilePath = "Assets/fonts/Roboto/Roboto-Regular.ttf";

    private static SKCanvas ColorCanvas { get; set; } = null!;
    private static SKCanvas IdCanvas { get; set; } = null!;

    private static readonly SKTypeface? FontTypeFace;
    
    private static SKPaint BlurPaint { get; } = new()
    {
        ImageFilter = SKImageFilter.CreateBlur(35f, 35f),
        ColorFilter = SKColorFilter.CreateBlendMode(SKColors.Transparent, SKBlendMode.SrcIn)
        // IsAntialias = true
    };

    private static SKPaint Shadow { get; } = new()
    {
        ImageFilter = SKImageFilter.CreateBlur(10f, 10f),
        Color = new SKColor(0, 0, 0, 100),
    };

    private static readonly float[] BlurColorPos = new float[] { 0.0f, 1.0f };

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
    
    public static void DrawMicaRoundRect(SKPoint position, SKSize size, Vector4 radius, SKColor? renderId = null, SKColor? splashColor = null, bool shadow = false)
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
        
        DrawMicaRRect(rect, roundedRect, splashColor, shadow);
        if(renderId != null) DrawRRectId(rect, roundedRect, (SKColor)renderId!);
    }
    
    public static void DrawMicaRoundRect(SKPoint position, SKSize size, float radius, SKColor? renderId = null, SKColor? splashColor = null, bool shadow = false)
    {
        var rect = new SKRect(position.X, position.Y, position.X + size.Width, position.Y + size.Height);
        var roundedRect = new SKRoundRect(rect , radius);
        
        DrawMicaRRect(rect, roundedRect, splashColor, shadow);
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
        position.Y += (fontSize - paint.FontMetrics.Descent) / 2;
        ColorCanvas.DrawText(text, position, paint);
    }
    
    public static float CalculateTextWidth(string text, float fontSize)
    {
        using var paint = new SKPaint();
        paint.Typeface = FontTypeFace;
        paint.TextSize = fontSize;
        return paint.MeasureText(text);
    }

    public static void DrawGrid(SKPoint position, SKSize size, SKPoint cameraOffset, float zoom, SKColor color)
    {
        Shaders.GridShader.SetUniform("zoom", zoom);
        Shaders.GridShader.SetUniform("cameraOffset", cameraOffset);
        Shaders.GridShader.SetUniform("color", color);
        
        var paint = new SKPaint
        {
            Shader = Shaders.GridShader.Get,
            IsAntialias = true
        };

        var rect = new SKRect(position.X, position.Y, position.X + size.Width, position.Y + size.Height);
        ColorCanvas.DrawRect(rect, paint);
    }

    public static void DrawCubicBezier(SKPoint start, SKPoint end, SKColor color, float weight, SKColor? idColor = null)
    {
        var dX = Math.Abs(start.X - end.X);
        var dY = Math.Abs(start.X - end.X);

        var offsetX = dX * (float)0.35;

        var controlPoint1 = new SKPoint(start.X + offsetX, start.Y);
        var controlPoint2 = new SKPoint(end.X - offsetX, end.Y);
        
        SKPath path = new();
        path.MoveTo(start);
        if (dX <= 0.35 || dY <= 0.35)
            path.LineTo(end);
        else
            path.CubicTo(controlPoint1, controlPoint2, end);
        
        using var paint = new SKPaint();
        paint.IsAntialias = true;
        paint.Color = color;
        paint.IsStroke = true;
        paint.StrokeWidth = weight;
        
        ColorCanvas.DrawPath(path, paint);
        
        if(idColor == null) return;
        
        paint.IsAntialias = false;
        paint.Color = (SKColor)idColor!;
        
        IdCanvas.DrawPath(path, paint);
    }
    

    private static void DrawRRectId(SKRect rect, SKRoundRect roundedRect, SKColor id)
    {
        using var idPaint = new SKPaint();
        idPaint.Color = id;
        IdCanvas.DrawRoundRect(roundedRect, idPaint);
    }
    
    private static void DrawMicaRRect(SKRect rect, SKRoundRect roundedRect, SKColor? splashColor = null, bool shadow = false)
    {
        using (var gradientPaint = new SKPaint())
        {
            gradientPaint.IsAntialias = true;
            gradientPaint.Shader = SKShader.CreateLinearGradient(
                new SKPoint(rect.Left, rect.Top),
                new SKPoint(rect.Right, rect.Bottom),
                new[] { new SKColor(40, 40, 40, 100), new SKColor(80, 80, 80, 200) },
                BlurColorPos,
                SKShaderTileMode.Clamp
            );
        
            ColorCanvas.DrawRoundRect(roundedRect, gradientPaint);
        }
        
        if (shadow)
        {
            roundedRect.Offset(8, 8);
            roundedRect.Deflate(4, 4);
            ColorCanvas.DrawRoundRect(roundedRect, Shadow);
            roundedRect.Offset(-8, -8);
            roundedRect.Inflate(4, 4);
        }
        
        // ColorCanvas.DrawRoundRect(roundedRect, BlurPaint);

        using (var overlayPaint = new SKPaint())
        {
            var color = splashColor ?? SKColors.White;
            overlayPaint.Color = color.WithAlpha(40);
            overlayPaint.BlendMode = SKBlendMode.SrcOver;
            overlayPaint.IsAntialias = true;
            
            ColorCanvas.DrawRoundRect(roundedRect, overlayPaint);
        }
    }
}