using System.Numerics;

namespace BessScene.SceneCore.State.Events;

public class MouseMoveEvent : Event<MouseMoveData>
{
    public MouseMoveEvent(float x, float y): base(EventType.MouseMove)
    {
        Data = new MouseMoveData(new Vector2(x, y));
    }
    
    public MouseMoveEvent(MouseMoveData data) : base(EventType.MouseMove, data)
    {
    }
}

public class MouseButtonEvent : Event<MouseButtonEventData>
{
    public MouseButtonEvent(MouseButton button, float x, float y, bool pressed) : base(EventType.MouseButton)
    {
        Data = new MouseButtonEventData(new Vector2(x, y), button, pressed);
    }
    
    public MouseButtonEvent(MouseButtonEventData data) : base(EventType.MouseButton, data)
    {
    }
}