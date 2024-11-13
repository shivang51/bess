namespace BessSimEngine.Components.Slots;


public abstract class Slot: Component
{
    public Guid ParentId { get; }
    public readonly List<Guid> Connections;
    
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
    
    public int ConnectionCount => Connections.Count;
    
    protected Component? Parent => SimEngineState.Instance.Components.Find(c => c.Id == ParentId);
    
    public bool IsInput => _type == SlotType.Input;
    
    public bool IsOutput => _type == SlotType.Output;
    
    public bool IsHigh => State == DigitalState.High;
    
    public void AddConnection(Guid id)
    {
        Connections.Add(id);
    }
    
    public void RemoveConnection(Guid id)
    {
        Connections.Remove(id);
    }

    public int StateInt => (int)State;

    public virtual void SetState(DigitalState state)
    {
        State = state;
        Simulate();

        var entry = new ChangeEntry(ParentId, _ind, StateInt, _type == SlotType.Input);
        SimEngineState.Instance.AddChangeEntry(entry);
    }

    public override List<List<int>> GetState()
    {
        return new List<List<int>>() { new(){ StateInt }};
    }
    
    public override void Remove()
    {
        foreach (var connection in Connections)
        {
            var slot = SimEngineState.Instance.GetSlot(connection);
            slot.RemoveConnection(Id);
        }
        SimEngineState.Instance.Slots.Remove(this);
    }
}