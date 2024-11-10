namespace BessSimEngine.Components.Slots;

public class InputSlot: Slot
{
    public InputSlot(Guid parentId) : base(parentId, SlotType.Input)
    {
    }

    public override void Simulate()
    {
        Parent?.ScheduleSim();
    }
    
    public override void SetState(DigitalState state)
    {
        State = state;
        Simulate();
    }
}