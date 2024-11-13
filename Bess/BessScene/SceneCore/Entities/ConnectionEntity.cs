using System.Numerics;
using BessScene.SceneCore.State;

namespace BessScene.SceneCore.Entities;

public abstract class ConnectionEntity: SceneEntity
{
    public uint StartSlotId { get; set; }
    public uint EndSlotId { get; set; }
    
    protected ConnectionEntity(uint startSlotId, uint endSlotId) : base(new Vector2(), new Vector2())
    {
        StartSlotId = startSlotId;
        EndSlotId = endSlotId;
        
        SceneState.Instance.AddConnectionEntity(this);
        
        SceneState.Instance.AddToConnectionMap(StartSlotId, RenderId);
        SceneState.Instance.AddToConnectionMap(EndSlotId, RenderId);
        
    }
    
    public override void Remove()
    {
        SceneState.Instance.RemoveFromConnectionMap(StartSlotId, RenderId);
        SceneState.Instance.RemoveFromConnectionMap(EndSlotId, RenderId);
        
        SceneState.Instance.RemoveConnectionEntity(this);
    }
}