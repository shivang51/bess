using System.Numerics;

namespace BessScene.SceneCore.State;

public class DragData
{
    public bool IsDragging { get; set; }
    public Vector2 StartPosition { get; set; }
    public Vector2 DragOffset { get; set; }
    public uint EntityId { get; set; }
}