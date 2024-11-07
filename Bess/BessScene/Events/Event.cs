namespace BessScene.SceneCore.State.Events;


public abstract class SceneEvent
{
    public EventType Type { get; protected set; }
    
    protected object? EventData;
    
    protected SceneEvent(EventType type, object data)
    {
        Type = type;
        EventData = data;
    }
    
    protected SceneEvent(EventType type)
    {
        Type = type;
        EventData = null;
    }
}

public abstract class Event<T>: SceneEvent where T : class
{
    public T Data
    {
        get
        {
            if (EventData == null) throw new NullReferenceException("Data is null");
            return (T)EventData;
        } 
        protected init => EventData = value;
    }
    
    protected Event(EventType type, T data) : base(type, data)
    {
    }
    
    protected Event(EventType type) : base(type)
    {
    }
}