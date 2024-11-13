namespace BessScene.SceneCore.State;

public class NewConnectionEntry
{
    public uint StartParentId { get; }
    public int StartSlotIndex { get; }
    public uint EndParentId { get; }
    public int EndSlotIndex { get; }
    
    public bool IsStartSlotInput { get; }
    
    public NewConnectionEntry(uint startParentId, int startSlotIndex, uint endParentId, int endSlotIndex, bool isStartSlotInput)
    {
        StartParentId = startParentId;
        StartSlotIndex = startSlotIndex;
        EndParentId = endParentId;
        EndSlotIndex = endSlotIndex;
        IsStartSlotInput = isStartSlotInput;
    }
}