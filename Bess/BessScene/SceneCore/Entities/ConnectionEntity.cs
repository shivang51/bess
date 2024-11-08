using System.Numerics;
using BessScene.SceneCore.ShadersCollection;

namespace BessScene.SceneCore.Entities;

public abstract class ConnectionEntity: SceneEntity
{
    public uint StartSlotId { get; set; }
    public uint EndSlotId { get; set; }
    
    protected ConnectionEntity(uint startSlotId, uint endSlotId) : base(new Vector2(), new Vector2())
    {
        StartSlotId = startSlotId;
        EndSlotId = endSlotId;
        
        SceneState.Instance.ConnectionEntities.Add(this);
    }
}