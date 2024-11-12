namespace BessSimEngine;

public class ChangeEntry
{
    public Guid ComponentId { get;}
    
    public int SlotIndex { get; }

    public int State { get; }

    public ChangeEntry(Guid componentId, int slotIndex, int state)
    {
        ComponentId = componentId;
        SlotIndex = slotIndex;
        State = state;
    }
}