namespace BessSimEngine;

public class ChangeEntry
{
    public Guid ComponentId { get;}
    
    public int SlotIndex { get; }

    public int State { get; }
    
    public bool IsInputSlot { get; }

    public ChangeEntry(Guid componentId, int slotIndex, int state, bool isInputSlot)
    {
        ComponentId = componentId;
        SlotIndex = slotIndex;
        State = state;
        IsInputSlot = isInputSlot;
    }
}