namespace BessSimEngine.Components.Slots;

public class OutputSlot: Slot
{
    public OutputSlot(Guid parentId) : base(parentId, SlotType.Output)
    {
    }

    public override void Simulate()
    {
        throw new NotImplementedException();
    }
}