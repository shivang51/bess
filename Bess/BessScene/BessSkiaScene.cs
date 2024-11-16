using System;
using System.Numerics;
using BessScene.SceneCore.Events;
using BessScene.SceneCore.Entities;
using BessScene.SceneCore.Sketches;
using SkiaSharp;

namespace BessScene.SceneCore;

public class BessSkiaScene
{
    private int Width { get; set; }
    private int Height { get; set; }
    
    private SKBitmap _colorBuffer = null!;
    private SKBitmap _idBuffer = null!;
    
    private uint EmptyId { get; } = 0;

    public CameraController Camera { get; set; } = new();
    
    public BessSkiaScene(double width, double height)
    {
        Width = (int)width;
        Height = (int)height;
        InitializeBuffers();

        SceneState.Instance.Camera = Camera;
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
        Camera.Resize(new Vector2(Width, Height));
        _colorBuffer?.Dispose();
        _idBuffer?.Dispose();
        InitializeBuffers();
    }

    public void Update(List<SceneEvent> events)
    {
        foreach (var evt in events)
        {
            switch (evt.Type)
            {
                case EventType.MouseMove:
                    OnMouseMove((evt as MouseMoveEvent)!.Data);
                break;
                case EventType.MouseButton:
                {
                    var data = (evt as MouseButtonEvent)!.Data;
                    switch (data.Button)
                    {
                        case MouseButton.Left:
                            OnMouseLeftEvent(data);
                            break;
                        case MouseButton.Middle:
                            OnMouseMiddleEvent(data);
                            break;
                        case MouseButton.Right:
                            break;
                        default:
                            throw new NotImplementedException($"{data.Button} mouse button is not implemented");
                    }
                }
                break;
                // case EventType.MouseScroll:
                //     break;
                // case EventType.KeyDown:
                //     break;
                // case EventType.KeyUp:
                //     break;
                // case EventType.KeyPress:
                //     break;
                // case EventType.WindowResize:
                    // break;
                default:
                    throw new ArgumentOutOfRangeException($"Invalid event type {evt.Type}");
            }
        }
        
        var rid = GetRenderObjectId((int)SceneState.Instance.MousePosition.X, (int)SceneState.Instance.MousePosition.Y);
        SceneState.Instance.SetHoveredEntityId(rid);
        
        var approvedConnections = SceneState.Instance.ApprovedConnectionEntries;
        
        foreach (var entry in approvedConnections)
        {
            var startEntity = SceneState.Instance.GetEntityByRenderId<GateSketch>(entry.StartParentId);
            var endEntity = SceneState.Instance.GetEntityByRenderId<GateSketch>(entry.EndParentId);
            uint startSlotId;
            uint endSlotId;
            if (entry.IsStartSlotInput)
            {
                startSlotId = startEntity.GetInputSlotIdAt(entry.StartSlotIndex);
                endSlotId = endEntity.GetOutputSlotIdAt(entry.EndSlotIndex);
            }
            else
            {
                startSlotId = startEntity.GetOutputSlotIdAt(entry.StartSlotIndex);
                endSlotId = endEntity.GetInputSlotIdAt(entry.EndSlotIndex);
            }
            _ = new ConnectionSketch(startSlotId, endSlotId);
        }
        
        foreach (var ent in SceneState.Instance.Entities.Values)
        {
            ent.Update();
        }
        
        foreach (var ent in SceneState.Instance.SlotEntities.Values)
        {
            ent.Update();
        }
        
        foreach (var ent in SceneState.Instance.ConnectionEntities.Values)
        {
            ent.Update();
        }
        
        foreach (var ent in SceneState.Instance.ConnectionSegments.Values)
        {
            ent.Update();
        }
        
        SceneState.Instance.ApprovedConnectionEntries.Clear();
        SceneState.Instance.SceneChangeEntries.Clear();
    }

    public SKBitmap GetColorBuffer() => _colorBuffer;

    public void RenderScene()
    {
        
        var colorCanvas = new SKCanvas(_colorBuffer);
        var idCanvas = new SKCanvas(_idBuffer);
        var backgroundColor = new SKColor(30, 30, 30, 200);
        colorCanvas.Clear(backgroundColor);
        idCanvas.Clear(new SKColor(0, 0, 0, 0));

        SkRenderer.Begin(colorCanvas, idCanvas);

        var gridColor = backgroundColor.Multiply(2).WithAlpha(255);
        
        SkRenderer.DrawGrid(
            new SKPoint(0, 0),
            new SKSize(Width, Height), 
            Camera.PositionSkPoint, 
            Camera.Zoom.X, 
            gridColor
        );
        
        colorCanvas.Scale(Camera.GetZoomSkPoint);
        idCanvas.Scale(Camera.GetZoomSkPoint);
        
        var position = VectorToSkPoint(Camera.Position);
        colorCanvas.Translate(position);
        idCanvas.Translate(position);
        
        SkRenderer.Begin(colorCanvas, idCanvas);

        foreach (var connection in SceneState.Instance.ConnectionEntities.Values)
        {
            connection.Render();
        }
        
        var connectionData = SceneState.Instance.ConnectionData;
        if (connectionData.IsConnecting)
        {
            SkRenderer.DrawCubicBezier(connectionData.StartPoint, ToWorldPos(SceneState.Instance.MousePosition), SKColors.Bisque, 1.5f);
        }
        
        foreach (var ent in SceneState.Instance.Entities.Values)
        {
            ent.Render();
        }

        foreach (var ent in SceneState.Instance.SlotEntities.Values)
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

        return new SKColor(0);
        // throw new ArgumentOutOfRangeException($"Pixel coordinates ({x} or {y}) are out of bounds.");
    }
    
    private static uint UIntFromRgba(byte r, byte g, byte b, byte a)
    {
        var id = (uint)r << 16 | (uint)g << 8 | (uint)b << 0 | 0;
        return id;
    } 
    
    private static SKPoint VectorToSkPoint(Vector2 vector) => new((float)vector.X, (float)vector.Y);

    private void OnMouseMove(MouseMoveData data)
    {
        var prevPos = SceneState.Instance.MousePosition;
        var dPos = data.Position - prevPos;
        SceneState.Instance.MousePosition = data.Position;
        var mousePos = SceneState.Instance.MousePosition;

        var dragData = SceneState.Instance.DragData;

        if (SceneState.Instance.IsLeftMousePressed)
        {
            if (dragData.IsDragging)
            {
                var ent = SceneState.Instance.GetDraggableEntity<DraggableSceneEntity>(dragData.EntityId);
                ent.UpdatePosition(ToWorldPos(mousePos) - dragData.DragOffset);
            }
        }
        else if (SceneState.Instance.IsMiddleMousePressed)
        {
            Camera.UpdatePositionBy(dPos);
        }
    }

    private Vector2 ToWorldPos(Vector2 pos)
    {
        return Transform.ScaleAndTranslate(pos, Camera.ZoomInv, -Camera.Position);
    }

    private void OnMouseLeftEvent(MouseButtonEventData data)
    {
        var pos = data.Position;
        var rid = GetRenderObjectId((int)pos.X, (int)pos.Y);
        SceneState.Instance.SelectedEntityId = rid;
        SceneState.Instance.MousePosition = pos;
        SceneState.Instance.IsLeftMousePressed = data.Pressed;

        if (SceneState.Instance.SelectedEntityId == EmptyId)
        {
            if (SceneState.Instance.ConnectionData.IsConnecting)
            {
                SceneState.Instance.EndConnection();
            }
            return;
        }

        if (data.Pressed)
        {
            if (SceneState.Instance.IsDraggableEntity(rid))
            {
                SceneState.Instance.StartDrag(rid, ToWorldPos(pos));
            }
            else if (SceneState.Instance.IsSlotEntity(rid))
            {
                if (!SceneState.Instance.ConnectionData.IsConnecting)
                {
                    SceneState.Instance.StartConnection(rid);
                }
                else
                {
                    SceneState.Instance.ConnectionData.EndEntityId = rid;
                    SceneState.Instance.EndConnection(true);
                }
            }
        }else
        {
            if(SceneState.Instance.DragData.IsDragging)
                SceneState.Instance.EndDrag();
        }
    }
    
    private void OnMouseMiddleEvent(MouseButtonEventData data)
    {
        SceneState.Instance.IsMiddleMousePressed = data.Pressed;
    }
}
