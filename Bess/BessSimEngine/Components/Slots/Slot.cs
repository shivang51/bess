namespace BessSimEngine.Components.Slots;


public abstract class Slot: Component
{
    protected Guid ParentId;
    protected List<Guid> Connections;
    
    public DigitalState State { get; private set; }

    private readonly SlotType _type;
    
    private readonly int _ind;
    
    
    
    protected Slot(Guid parentId, SlotType type, int ind): base("Slot", 0, 0)
    {
        _type = type;
        ParentId = parentId;
        Connections = new List<Guid>();
        _ind = ind;    
        SimEngineState.Instance.Slots.Add(this);
    }
    
    protected Component? Parent => SimEngineState.Instance.Components.Find(c => c.Id == ParentId);
    
    protected bool IsInput => _type == SlotType.Input;
    
    protected bool IsOutput => _type == SlotType.Output;
    
    public bool IsHigh => State == DigitalState.High;
    
    public void AddConnection(Guid id)
    {
        Connections.Add(id);
    }

    public int StateInt => (int)State;
    
    public bool IsInputSlot => _type == SlotType.Input;

    public virtual void SetState(DigitalState state)
    {
        State = state;
        Simulate();

        var entry = new ChangeEntry(ParentId, _ind, StateInt);
        SimEngineState.Instance.AddChangeEntry(entry);
    }

    public override List<List<int>> GetState()
    {
        return new List<List<int>>() { new(){ StateInt }};
    }
}