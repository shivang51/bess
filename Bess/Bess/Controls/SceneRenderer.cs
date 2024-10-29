using System;
using Avalonia;
using SkiaSharp;

namespace Bess.Controls;

public class SceneRenderer
{
    private int Width { get; set; }
    private int Height { get; set; }
    
    private SKBitmap _colorBuffer = null!;
    private SKBitmap _idBuffer = null!;

    public SceneRenderer(double width, double height)
    {
        Width = (int)width;
        Height = (int)height;
        InitializeBuffers();
    }

    private void InitializeBuffers()
    {
        _colorBuffer = new SKBitmap(Width, Height, SKColorType.Rgba8888, SKAlphaType.Premul);
        _idBuffer = new SKBitmap(Width, Height, SKColorType.Rgba8888, SKAlphaType.Opaque);
    }

    public void Resize(double newWidth, double newHeight)
    {
        var w = (int)newWidth;
        var h = (int)newHeight;
        if (w == Width && h == Height) return;
        Width = w;
        Height = h;
        _colorBuffer?.Dispose();
        _idBuffer?.Dispose();
        InitializeBuffers();
    }

    public SKBitmap GetColorBuffer() => _colorBuffer;

    public void RenderScene(CameraController cameraController)
    {
        using var colorCanvas = new SKCanvas(_colorBuffer);
        using var idCanvas = new SKCanvas(_idBuffer);
        
        colorCanvas.Clear(new SKColor(30, 30, 30, 255));
        idCanvas.Clear(new SKColor(0, 0, 0, 255));

        colorCanvas.Scale(cameraController.GetZoomPoint());
        idCanvas.Scale(cameraController.GetZoomPoint());
        
        var position = VectorToSkPoint(cameraController.GetPosition());
        colorCanvas.Translate(position);
        idCanvas.Translate(position);

        using (var paint = new SKPaint())
        {
            paint.Color = SKColors.Gray;
            paint.IsAntialias = true;
            colorCanvas.DrawCircle(50, 50, 30, paint);
        }

        using (var idPaint = new SKPaint())
        {
            idPaint.Color = new SKColor(150, 0, 0); // ID color
            idCanvas.DrawCircle(50, 50, 30, idPaint);
        }

        colorCanvas.Flush();
        idCanvas.Flush();
    }

    public int GetRenderObjectId(int x, int y)
    {
        var color = ReadPixel(_idBuffer, x, y);
        return color.Red;
    }
    
    private static SKColor ReadPixel(SKBitmap bitmap, int x, int y)
    {
        if (x >= 0 && x < bitmap.Width && y >= 0 && y < bitmap.Height)
        {
            return bitmap.GetPixel(x, y);
        }
        
        throw new ArgumentOutOfRangeException($"Pixel coordinates ({x} or {y}) are out of bounds.");
    }
    
    private static SKPoint VectorToSkPoint(Vector vector) => new((float)vector.X, (float)vector.Y);
}