using System;
using System.Numerics;
using BessScene.SceneCore;
using SkiaSharp;

namespace BessScene;

public class BessSkiaScene
{
    private int Width { get; set; }
    private int Height { get; set; }
    
    private SKBitmap _colorBuffer = null!;
    private SKBitmap _idBuffer = null!;

    public BessSkiaScene(double width, double height)
    {
        Width = (int)width;
        Height = (int)height;
        InitializeBuffers();
    }

    private void InitializeBuffers()
    {
        _colorBuffer = new SKBitmap(Width, Height, SKColorType.Rgba8888, SKAlphaType.Premul);
        _idBuffer = new SKBitmap(Width, Height, SKColorType.Rgba8888, SKAlphaType.Premul);
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
        var colorCanvas = new SKCanvas(_colorBuffer);
        var idCanvas = new SKCanvas(_idBuffer);
        
        colorCanvas.Clear(new SKColor(30, 30, 30, 255));
        idCanvas.Clear(new SKColor(0, 0, 0, 0));

        colorCanvas.Scale(cameraController.GetZoomPoint());
        idCanvas.Scale(cameraController.GetZoomPoint());
        
        var position = VectorToSkPoint(cameraController.GetPosition());
        colorCanvas.Translate(position);
        idCanvas.Translate(position);

        SkRenderer.Begin(colorCanvas, idCanvas);
        
        foreach (var ent in SceneState.Instance.Entities)
        {
            ent.Render();
        }


        colorCanvas.Flush();
        idCanvas.Flush();
    }

    public uint GetRenderObjectId(int x, int y)
    {
        var color = ReadPixel(_idBuffer, x, y);
        return UIntFromRgba(color.Red, color.Green, color.Blue, color.Alpha);
    }
    
    private static SKColor ReadPixel(SKBitmap bitmap, int x, int y)
    {
        if (x >= 0 && x < bitmap.Width && y >= 0 && y < bitmap.Height)
        {
            return bitmap.GetPixel(x, y);
        }
        
        throw new ArgumentOutOfRangeException($"Pixel coordinates ({x} or {y}) are out of bounds.");
    }
    
    private static uint UIntFromRgba(byte r, byte g, byte b, byte a)
    {
        var id = (uint)r << 24 | (uint)g << 16 | (uint)b << 8 | a;
        return id;
    } 
    
    private static SKPoint VectorToSkPoint(Vector2 vector) => new((float)vector.X, (float)vector.Y);
}
