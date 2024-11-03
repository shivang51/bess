using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Numerics;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Input;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Markup.Xaml;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Skia;
using Avalonia.Threading;
using BessScene;
using CommunityToolkit.Mvvm.Input;
using SkiaSharp;

namespace Bess.Controls;

public class BessSceneControl: Control
{
    private readonly BessSkiaScene _scene;
    private readonly CameraController _cameraController;
    private readonly Dictionary<Key, bool> _keys = new();
    private static DispatcherTimer? _timer;
    private const int Fps = 120;
    private const int FrameRateMs = (1000 / Fps);
    
    public float Zoom
    {
        get => GetValue(ZoomProperty);
        set => SetValue(ZoomProperty!, value);
    }

    public RelayCommand<float>? OnZoomChanged
    {
        get => GetValue(OnZoomChangedProperty);
        set => SetValue(OnZoomChangedProperty!, value);
    }
    
    public static readonly StyledProperty<RelayCommand<float>> OnZoomChangedProperty = 
        AvaloniaProperty.Register<BessSceneControl, RelayCommand<float>>(nameof(OnZoomChanged));

    public static readonly StyledProperty<float> ZoomProperty = 
        AvaloniaProperty.Register<BessSceneControl, float>(nameof(Zoom));
    
    private Vector2 _cameraPosition;
    private CancellationTokenSource _cancellationTokenSource;

    public BessSceneControl()
    {
        _cancellationTokenSource = new CancellationTokenSource();
        _scene = new BessSkiaScene(Bounds.Width, Bounds.Height);
        _cameraController = new CameraController();
        
        _cameraPosition = _cameraController.GetPosition();
        
        KeyUpEvent.AddClassHandler<TopLevel>(OnKeyUp, handledEventsToo: true);
        KeyDownEvent.AddClassHandler<TopLevel>(OnKeyDown, handledEventsToo: true);
        
        StartTimer();
    }

    private void StartTimer()
    {
        _timer = new DispatcherTimer()
        {
            Interval = TimeSpan.FromMilliseconds(FrameRateMs)
        };
        _timer.Tick += (sender, args) => UpdateContent();
        _timer.Start();
    }

    private void UpdateContent()
    {
        UpdateCamera();
        _scene.Update();
        _scene.RenderScene(_cameraController);
        InvalidateVisual(); // Redraw control with updated buffers
    }

    private void UpdateCamera()
    {
        _cameraController.UpdateZoom(Zoom);
        _cameraController.SetPosition(_cameraPosition / (Zoom / 100));
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
        base.OnPointerWheelChanged(e); // Don't forget to call the base method if necessary

        var delta = new Vector2((float)e.Delta.X, (float)e.Delta.Y);

        if (IsKeyPressed(Key.LeftCtrl) || IsKeyPressed(Key.RightCtrl))
        {
            ThrottleZoomUpdate(delta.Y);
        }
        else
        {
            ThrottleCameraUpdate(delta);
        }
    }

    private void ThrottleZoomUpdate(float deltaY)
    {
        _cancellationTokenSource?.Cancel();
        _cancellationTokenSource = new CancellationTokenSource();

        Task.Delay(FrameRateMs, _cancellationTokenSource.Token).ContinueWith(t =>
        {
            if (t.IsCanceled) return;
            
            Dispatcher.UIThread.InvokeAsync(() =>
            {
                var dZoom = deltaY * CameraController.ZoomFactor * 100;
                _cameraController.UpdateZoom(Zoom + dZoom);
                Zoom = _cameraController.ZoomPercentage;
                OnZoomChanged?.Execute(Zoom);
            });

        });
    }

    private void ThrottleCameraUpdate(Vector2 delta)
    {
        _cancellationTokenSource?.Cancel();
        _cancellationTokenSource = new CancellationTokenSource();

        Task.Delay(FrameRateMs, _cancellationTokenSource.Token).ContinueWith(t =>
        {
            if (t.IsCanceled) return;
            _cameraPosition += delta * 20;
        });
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

    protected override void OnPointerMoved(PointerEventArgs e)
    {
        base.OnPointerMoved(e);
        var pos = e.GetCurrentPoint(this).Position;
        SceneState.Instance.SetMousePosition((float)pos.X, (float)pos.Y);
    }
}
