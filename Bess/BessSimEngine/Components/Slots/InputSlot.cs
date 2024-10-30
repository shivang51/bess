namespace BessSimEngine.Components.Slots;

public class InputSlot: Slot
{
    public InputSlot(Guid parentId) : base(parentId, SlotType.Input)
    {
    }

    public override void Simulate()
    {
        throw new NotImplementedException();
    }
}