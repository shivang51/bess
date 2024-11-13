using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Globalization;
using System.IO;
using System.Linq;
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
using Bess.Models.ComponentExplorer;
using BessScene.SceneCore;
using BessScene.SceneCore.Events;
using BessScene.SceneCore.State;
using BessSimEngine;
using BessSimEngine.Components;
using CommunityToolkit.Mvvm.Input;
using SkiaSharp;
using MouseButton = BessScene.SceneCore.Events.MouseButton;

namespace Bess.Controls;

public class BessSceneControl: Control
{
    private readonly BessSkiaScene _scene;
    private readonly Dictionary<Key, bool> _keys = new();
    private static DispatcherTimer? _timer;
    private const int Fps = 120;
    private const int FrameRateMs = (1000 / Fps);
    
    private readonly Dictionary<EventType, SceneEvent> _sceneEventsDict = new();
    
    private void AddSceneEvent(SceneEvent sceneEvent)
    {
        _sceneEventsDict[sceneEvent.Type] = sceneEvent;
    }
    
    public float Zoom
    {
        get => GetValue(ZoomProperty);
        set => SetValue(ZoomProperty!, value);
    }
    
    public List<AddedComponent> AddedComponents
    {
        get => GetValue(AddedComponentsProperty);
        set => SetValue(AddedComponentsProperty!, value);
    }

    public CornerRadius Radius
    {
        get => GetValue(CornerRadiusProperty);
        set => SetValue(CornerRadiusProperty!, value);
    }
    
    public RelayCommand<float>? OnZoomChanged
    {
        get => GetValue(OnZoomChangedProperty);
        set => SetValue(OnZoomChangedProperty!, value);
    }
    
    public static readonly StyledProperty<RelayCommand<float>> OnZoomChangedProperty = 
        AvaloniaProperty.Register<BessSceneControl, RelayCommand<float>>(nameof(OnZoomChanged));

    public static readonly StyledProperty<CornerRadius> CornerRadiusProperty = 
        AvaloniaProperty.Register<BessSceneControl, CornerRadius>(nameof(Radius));
    
    public static readonly StyledProperty<float> ZoomProperty = 
        AvaloniaProperty.Register<BessSceneControl, float>(nameof(Zoom));
    
    public static readonly StyledProperty<List<AddedComponent>> AddedComponentsProperty = 
        AvaloniaProperty.Register<BessSceneControl, List<AddedComponent>>(nameof(AddedComponents));
    
    private Vector2 _cameraPosition;
    private CancellationTokenSource _cancellationTokenSource;

    private BessSimEngine.SimEngine _simEngine;

    public BessSceneControl()
    {
        _cancellationTokenSource = new CancellationTokenSource();
        _scene = new BessSkiaScene(Bounds.Width, Bounds.Height);
        
        _cameraPosition = _scene.Camera.Position;
        
        KeyUpEvent.AddClassHandler<TopLevel>(OnKeyUp, handledEventsToo: true);
        KeyDownEvent.AddClassHandler<TopLevel>(OnKeyDown, handledEventsToo: true);
        
        StartTimer();
        _simEngine = new BessSimEngine.SimEngine();
        _simEngine.Start();
    }

    private void StartTimer()
    {
        _timer = new DispatcherTimer()
        {
            Interval = TimeSpan.FromMilliseconds(FrameRateMs)
        };
        _timer.Tick += (_, __) => UpdateContent();
        _timer.Start();
    }

    private void UpdateContent()
    {
        UpdateChangesFromScene();
        UpdateChangesFromSimEngine();
        UpdateCamera();
        _scene.Update(_sceneEventsDict.Values.ToList());
        _sceneEventsDict.Clear();
        _scene.RenderScene();
        InvalidateVisual(); // Redraw control with updated buffers
        _cameraPosition = _scene.Camera.Position;
        Zoom = _scene.Camera.ZoomPercentage;
    }

    private static void UpdateChangesFromSimEngine()
    {
        SimEngineState.Instance.FillChangeEntries(out var changeEntries);
        
        if(changeEntries.Count == 0) return;

        Dictionary<uint, List<SceneChangeEntry>> sceneChangeEntries = new();
        
        foreach (var entry in changeEntries)
        {
            var rid = AddedComponent.ComponentIdToRenderId[entry.ComponentId];
            var sceneChangeEntry = new SceneChangeEntry(rid, entry.SlotIndex, entry.State == 1, entry.IsInputSlot);
         
            if (sceneChangeEntries.TryGetValue(rid, out var entries))
            {
                entries.Add(sceneChangeEntry);
            }
            else
            {
                sceneChangeEntries[rid] = [sceneChangeEntry];
            }
        }
        
        SceneState.Instance.SceneChangeEntries = sceneChangeEntries; 
    }
    
    private static void UpdateChangesFromScene()
    {
        SceneState.Instance.FillNewConnectionEntry(out var newConnectionEntries);

        foreach (var connectionEntry in newConnectionEntries)
        {
            var startId = AddedComponent.RenderIdToComponentId[connectionEntry.StartParentId];
            var endId = AddedComponent.RenderIdToComponentId[connectionEntry.EndParentId];
            var success = SimEngine.Connect(startId, connectionEntry.StartSlotIndex, endId, connectionEntry.EndSlotIndex, connectionEntry.IsStartSlotInput);
            if(!success) continue;
            Console.WriteLine("Connection added");
            SceneState.Instance.ApprovedConnectionEntries.Add(connectionEntry);
        }
    }

    private void UpdateCamera()
    {
        _scene.Camera.SetZoomPercentage(Zoom);
        _scene.Camera.SetPosition(_cameraPosition);
    }

    public override void Render(DrawingContext context)
    {
        var rect = new Rect(0, 0, Bounds.Width, Bounds.Height);
        var geometry = new RoundedRect(rect, Radius);

        using (context.PushClip(geometry))
        {
            context.DrawImage(ColorBuffer, new Rect(0, 0, rect.Width, rect.Height));
        }
    }

    private Bitmap ColorBuffer => ConvertToAvaloniaBitmap(_scene.GetColorBuffer());
    
    private static Bitmap ConvertToAvaloniaBitmap(SKBitmap skBitmap)
    {
        using var image = SKImage.FromBitmap(skBitmap);
        if (image == null) return new Bitmap(Stream.Null);
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

    protected override void OnPointerReleased(PointerReleasedEventArgs e)
    {
        base.OnPointerReleased(e);
        OnMouseButtonEvent(e, false);
    }
    
    protected override void OnPointerPressed(PointerPressedEventArgs e)
    {
        base.OnPointerPressed(e);
        OnMouseButtonEvent(e, true);
    }

    private void OnMouseButtonEvent(PointerEventArgs e, bool pressed)
    {
        var properties = e.GetCurrentPoint(this).Properties;
        var pos = e.GetCurrentPoint(this).Position;
        MouseButton? button = properties.PointerUpdateKind switch
        {
            PointerUpdateKind.LeftButtonReleased => MouseButton.Left,
            PointerUpdateKind.RightButtonReleased => MouseButton.Right,
            PointerUpdateKind.MiddleButtonReleased => MouseButton.Middle,
            PointerUpdateKind.LeftButtonPressed => MouseButton.Left,
            PointerUpdateKind.RightButtonPressed => MouseButton.Right,
            PointerUpdateKind.MiddleButtonPressed => MouseButton.Middle,
            _ => null
        };

        if (button == null)
        {
            throw new InvalidDataException($"Invalid button {properties.PointerUpdateKind}");
        }
        
        var mouseEvent = new MouseButtonEvent((MouseButton)button!, (float)pos.X, (float)pos.Y, pressed);
        AddSceneEvent(mouseEvent);
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
                Zoom = CameraController.ConstrainZoomPercentage(Zoom + dZoom);
                _scene.Camera.UpdateZoomPercentage(Zoom);
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
            Dispatcher.UIThread.InvokeAsync(() =>
            {
                _cameraPosition += delta * 10;
                _scene.Camera.SetPosition(_cameraPosition);
            });
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
        AddSceneEvent(new MouseMoveEvent((float)pos.X, (float)pos.Y));
    }
}
