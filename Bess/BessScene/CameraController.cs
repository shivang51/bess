using System.Numerics;
using SkiaSharp;

namespace BessScene.SceneCore;

public class CameraController
{
    public const float DefaultZoom = (float)1.4;
    public const float MaxZoom = (float)2.5;
    public const float ZoomFactor = (float)0.1;
    public const float MinZoom = (float)0.5;

    private readonly Vector2 _defaultZoomVec = new(DefaultZoom);
    
    public Vector2 Position { get; private set; }
    
    public Vector2 Zoom { get; private set; }
    
    public CameraController()
    {
        ResetCamera();
    }
    
    public void SetPosition(Vector2 position)
    {
        Position = position;
    }
    
    public void SetZoomPercentage(float zoom)
    {
        SetZoom(zoom / 100);
    }
    
    /// <summary>
    /// Adds given value to current position
    /// </summary>
    /// <param name="dPos"></param>
    public void UpdatePositionBy(Vector2 dPos)
    {
        Position += dPos;
    }
    
    public float ZoomPercentage => (Zoom.X) * 100;
    
    public SKPoint GetZoomSkPoint => new(Zoom.X, Zoom.Y);
    
    public Vector2 ZoomInv => new(1 / Zoom.X, 1 / Zoom.Y);
    
    public SKPoint PositionSkPoint => new((float)Position.X, (float)Position.Y);
    
    public void UpdateZoomPercentage(float percentage)
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
        Zoom = new Vector2(ConstrainZoom(val));
    }

    public static float ConstrainZoom(float val)
    {
        return Math.Clamp(val, MinZoom, MaxZoom);
    }
    
    public static float ConstrainZoomPercentage(float val)
    {
        return ConstrainZoom(val / 100) * 100;
    }
}