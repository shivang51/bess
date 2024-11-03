using System.Numerics;
using SkiaSharp;

namespace BessScene;

public class CameraController
{
    public const float DefaultZoom = (float)1.4;
    public const float MaxZoom = (float)2.5;
    public const float ZoomFactor = (float)1.1;
    public const float MinZoom = (float)0.5;

    private readonly Vector2 _defaultZoomVec = new(DefaultZoom);
    
    private Vector2 Position { get; set; }
    
    private Vector2 Zoom { get; set; }
    
    public CameraController()
    {
        ResetCamera();
    }
    
    public void SetPosition(Vector2 position)
    {
        Position = position;
    }
    
    public float ZoomPercentage => (Zoom.X) * 100;
    
    public SKPoint GetZoomPoint() => new((float)Zoom.X, (float)Zoom.Y);
    
    public Vector2 GetPosition() => Position;
    
    public SKPoint GetPositionPoint() => new((float)Position.X, (float)Position.Y);
    
    public void UpdateZoom(float percentage)
    {
        SetZoom(percentage / 100);
    }
    
    private void ResetCamera()
    {
        Position = new Vector2(0, 0);
        Zoom = _defaultZoomVec;
    }

    private void SetZoom(float val)
    {
        val = val switch
        {
            < MinZoom => MinZoom,
            > MaxZoom => MaxZoom,
            _ => val
        };

        Zoom = new Vector2(val);
    }
}