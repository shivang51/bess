using System.Numerics;

namespace BessScene.SceneCore.State.Events;

public class MouseMoveData
{
    public Vector2 Position { get; set; }
    
    public MouseMoveData(Vector2 position)
    {
        Position = position;
    }
}

public class MouseButtonEventData
{
    public Vector2 Position { get; set; }
    public MouseButton Button { get; set; }
    
    public bool Pressed { get; set; }
    
    public bool IsLeftButton => Button == MouseButton.Left;
    public bool IsRightButton => Button == MouseButton.Right;
    public bool IsMiddleButton => Button == MouseButton.Middle;
    
    public MouseButtonEventData(Vector2 position, MouseButton button, bool pressed)
    {
        Position = position;
        Button = button;
        Pressed = pressed;
    }
}