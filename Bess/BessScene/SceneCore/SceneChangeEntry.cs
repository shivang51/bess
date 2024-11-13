namespace BessScene.SceneCore;

public class SceneChangeEntry
{
    public uint RenderId { get; }
    
    public int SlotIndex { get; }
    
    public bool IsHigh { get; }
    
    public bool IsInputSlot { get; }
    
    public SceneChangeEntry(uint renderId, int slotIndex, bool isHigh, bool isInputSlot)
    {
        RenderId = renderId;
        SlotIndex = slotIndex;
        IsHigh = isHigh;
        IsInputSlot = isInputSlot;
    }
}