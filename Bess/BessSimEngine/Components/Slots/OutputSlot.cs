namespace BessSimEngine.Components.Slots;

public class OutputSlot: Slot
{
    public OutputSlot(Guid parentId) : base(parentId, SlotType.Output)
    {
    }

    public override void Simulate()
    {
        foreach (var component1 in Connections.Select(GetConnection))
        {
            var component = (InputSlot)component1!;
            component.SetState(State);
        }
    }
    
    public override void SetState(DigitalState state)
    {
        State = state;
        Simulate();
    }
}