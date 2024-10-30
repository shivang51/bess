using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Numerics;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Skia;
using Avalonia.Threading;
using BessScene;
using SkiaSharp;

namespace Bess.Controls;

public class BessSceneControl: Control
{
    private readonly BessSkiaScene _scene;
    private readonly CameraController _cameraController;
    private readonly Dictionary<Key, bool> _keys = new();
    private static DispatcherTimer? _timer;
    
    public BessSceneControl()
    {
        _scene = new BessSkiaScene(Bounds.Width, Bounds.Height);
        _cameraController = new CameraController();
        
        KeyUpEvent.AddClassHandler<TopLevel>(OnKeyUp, handledEventsToo: true);
        KeyDownEvent.AddClassHandler<TopLevel>(OnKeyDown, handledEventsToo: true);
        
        StartTimer();
    }
    
    private void StartTimer()
    {
        _timer = new DispatcherTimer()
        {
            Interval = TimeSpan.FromMilliseconds(1000.0 / 60.0)
        };
        _timer.Tick += (sender, args) => UpdateContent();
        _timer.Start();
    }

    private void UpdateContent()
    {
        _scene.RenderScene(_cameraController);
        InvalidateVisual(); // Redraw control with updated buffers
    }

    public override void Render(DrawingContext context)
    {
        context.DrawImage(ColorBuffer, new Rect(0, 0, Bounds.Width, Bounds.Height));
    }

    private Bitmap ColorBuffer => ConvertToAvaloniaBitmap(_scene.GetColorBuffer());
    
    private static Bitmap ConvertToAvaloniaBitmap(SKBitmap skBitmap)
    {
        using var image = SKImage.FromBitmap(skBitmap);
        using var data = image.Encode(SKEncodedImageFormat.Png, 100);
        using var stream = new MemoryStream(data.ToArray());
        return new Bitmap(stream);
    }

    protected override Size MeasureOverride(Size availableSize)
    {
        _scene.Resize(availableSize.Width, availableSize.Height);
        UpdateContent();   
        return base.MeasureOverride(availableSize);
    }

    protected override void OnPointerPressed(PointerPressedEventArgs e)
    {
        base.OnPointerPressed(e);
        var pos = e.GetCurrentPoint(this).Position;
        Console.WriteLine(_scene.GetRenderObjectId((int)pos.X, (int)pos.Y));
    }

    protected override void OnPointerWheelChanged(PointerWheelEventArgs e)
    {
        base.OnPointerWheelChanged(e);
        var delta = new Vector2((float)e.Delta.X, (float)e.Delta.Y);
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
