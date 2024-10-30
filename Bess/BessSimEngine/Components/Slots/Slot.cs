namespace BessSimEngine.Components.Slots;


public abstract class Slot
{
    protected Guid Id;
    protected Guid ParentId;
    protected List<Guid> Connections;
    
    public DigitalState State { get; set; }

    private readonly SlotType _type;
    
    protected Slot(Guid parentId, SlotType type)
    {
        Id = Guid.NewGuid();
        _type = type;
        ParentId = parentId;
        Connections = new List<Guid>();
    }
    
    protected bool IsInput => _type == SlotType.Input;
    
    protected bool IsOutput => _type == SlotType.Output;
    
    public bool IsHigh => State == DigitalState.High;
    
    public abstract void Simulate();
}