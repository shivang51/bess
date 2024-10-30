using System.Drawing;
using System.Numerics;
using SkiaSharp;

namespace BessScene;

public class CameraController
{
    private readonly Vector2 DefaultZoom = new Vector2(1, 1);
    private const float ZoomFactor = (float)1.1;
    
    private Vector2 Position { get; set; }
    
    private Vector2 Zoom { get; set; }
    
    public CameraController()
    {
        ResetCamera();
    }
    
    public void MoveCamera(Vector2 delta)
    {
        Position += delta;
    }
    
    public void SetCameraPosition(Vector2 position)
    {
        Position = position;
    }
    
    public Vector2 GetZoom() => Zoom;
    
    public SKPoint GetZoomPoint() => new((float)Zoom.X, (float)Zoom.Y);
    
    public Vector2 GetPosition() => Position;
    
    public SKPoint GetPositionPoint() => new((float)Position.X, (float)Position.Y);
    
    public void UpdateZoom(float factor)
    {
        Zoom = DefaultZoom * factor;
    }

    public void UpdateZoom(Vector2 delta)
    {
        Zoom = delta.Y > 0 ? Zoom * ZoomFactor : Zoom / ZoomFactor;
    }
    
    private void ResetCamera()
    {
        Position = new Vector2(0, 0);
        Zoom = DefaultZoom;
    }
}