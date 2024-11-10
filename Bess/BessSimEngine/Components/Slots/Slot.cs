namespace BessSimEngine.Components.Slots;


public abstract class Slot: Component
{
    protected Guid ParentId;
    protected List<Guid> Connections;
    
    public DigitalState State { get; set; }

    private readonly SlotType _type;
    
    protected Slot(Guid parentId, SlotType type): base("Slot", 0, 0)
    {
        _type = type;
        ParentId = parentId;
        Connections = new List<Guid>();
    }
    
    protected Component? Parent => SimEngineState.Instance.Components.Find(c => c.Id == ParentId);
    
    protected static Component? GetConnection(Guid id) => SimEngineState.Instance.Components.Find(c => c.Id == id);
    
    protected bool IsInput => _type == SlotType.Input;
    
    protected bool IsOutput => _type == SlotType.Output;
    
    public bool IsHigh => State == DigitalState.High;
    
    public void AddConnection(Guid id)
    {
        Connections.Add(id);
    }

    public abstract void SetState(DigitalState state);
}