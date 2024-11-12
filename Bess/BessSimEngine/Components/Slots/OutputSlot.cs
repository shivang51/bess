namespace BessSimEngine.Components.Slots;

public class OutputSlot: Slot
{
    public OutputSlot(Guid parentId, int ind) : base(parentId, SlotType.Output, ind)
    {
    }

    public override void Simulate()
    {
        foreach (var component1 in Connections.Select(SimEngineState.Instance.GetSlot))
        {
            var component = (InputSlot)component1!;
            component.SetState(State);
        }
    }
}