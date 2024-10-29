using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Skia;
using SkiaSharp;

namespace Bess.Controls;

public class BessScene: Control
{
    private readonly SceneRenderer _renderer;
    private readonly CameraController _cameraController;
    private readonly Dictionary<Key, bool> _keys = new();
    
    public BessScene()
    {
        _renderer = new SceneRenderer(Bounds.Width, Bounds.Height);
        _cameraController = new CameraController();
        
        KeyUpEvent.AddClassHandler<TopLevel>(OnKeyUp, handledEventsToo: true);
        KeyDownEvent.AddClassHandler<TopLevel>(OnKeyDown, handledEventsToo: true);
    }

    private void UpdateContent()
    {
        _renderer.RenderScene(_cameraController);
        InvalidateVisual(); // Redraw control with updated buffers
    }

    public override void Render(DrawingContext context)
    {
        context.DrawImage(ColorBuffer, new Rect(0, 0, Bounds.Width, Bounds.Height));
    }

    private Bitmap ColorBuffer => ConvertToAvaloniaBitmap(_renderer.GetColorBuffer());
    
    private static Bitmap ConvertToAvaloniaBitmap(SKBitmap skBitmap)
    {
        using var image = SKImage.FromBitmap(skBitmap);
        using var data = image.Encode(SKEncodedImageFormat.Png, 100);
        using var stream = new MemoryStream(data.ToArray());
        return new Bitmap(stream);
    }

    protected override Size MeasureOverride(Size availableSize)
    {
        _renderer.Resize(availableSize.Width, availableSize.Height);
        UpdateContent();   
        return base.MeasureOverride(availableSize);
    }

    protected override void OnPointerPressed(PointerPressedEventArgs e)
    {
        base.OnPointerPressed(e);
        var pos = e.GetCurrentPoint(this).Position;
        Console.WriteLine(_renderer.GetRenderObjectId((int)pos.X, (int)pos.Y));
    }

    protected override void OnPointerWheelChanged(PointerWheelEventArgs e)
    {
        base.OnPointerWheelChanged(e);
        var delta = e.Delta;
        if(IsKeyPressed(Key.LeftCtrl) || IsKeyPressed(Key.RightCtrl))
        {
            _cameraController.UpdateZoom(delta);
        }
        else
        {
            _cameraController.MoveCamera(delta * 20);
        }
        
        UpdateContent();
    }
    
    private void OnKeyDown(object? sender, KeyEventArgs e)
    {
        _keys[e.Key] = true;
    }

    private void OnKeyUp(object?  sender, KeyEventArgs e)
    {
        _keys[e.Key] = false;
    }

    private bool IsKeyPressed(Key key) => _keys.TryGetValue(key, out var isPressed) && isPressed;
}
