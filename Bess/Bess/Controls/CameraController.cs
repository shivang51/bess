using System;
using System.Collections.Specialized;
using Avalonia;
using Avalonia.Media;
using SkiaSharp;
using Vector = Avalonia.Vector;

namespace Bess.Controls;

public class CameraController
{
    private readonly Vector DefaultZoom = new Vector(1, 1);
    private const double ZoomFactor = 1.1;
    
    private Vector Position { get; set; }
    
    private Vector Zoom { get; set; }
    
    public MatrixTransform Transform => new(new Matrix(1 / Zoom.X, 0, 0, 1 / Zoom.Y, Position.X, Position.Y));
    
    public CameraController()
    {
        ResetCamera();
    }
    
    public void MoveCamera(Vector delta)
    {
        Position += delta;
    }
    
    public void MoveCamera(Point delta)
    {
        Position += delta;
    }
    
    public void SetCameraPosition(Point position)
    {
        Position = position;
    }
    
    public Vector GetZoom() => Zoom;
    
    public SKPoint GetZoomPoint() => new((float)Zoom.X, (float)Zoom.Y);
    
    public Vector GetPosition() => Position;
    
    public SKPoint GetPositionPoint() => new((float)Position.X, (float)Position.Y);
    
    public void UpdateZoom(float factor)
    {
        Zoom = DefaultZoom * factor;
    }

    public void UpdateZoom(Vector delta)
    {
        Zoom = delta.Y > 0 ? Zoom * ZoomFactor : Zoom / ZoomFactor;
    }
    
    private void ResetCamera()
    {
        Position = new Point(0, 0);
        Zoom = new Point(1, 1);
    }
}