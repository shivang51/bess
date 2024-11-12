namespace BessSimEngine.Components.Slots;

public class InputSlot: Slot
{
    public InputSlot(Guid parentId, int ind) : base(parentId, SlotType.Input, ind)
    {
    }

    public override void Simulate()
    {
        Parent?.ScheduleSim();
    }
}