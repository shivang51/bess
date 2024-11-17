using System.Numerics;
using BessScene.SceneCore.State;

namespace BessScene.SceneCore.Entities;

public abstract class DigitalEntity: SceneEntity
{
    protected List<uint> InputSlots = new();
    protected List<uint> OutputSlots = new();
    
    protected DigitalEntity(Transform transform, uint renderId) : base(transform, renderId)
    {
    }

    protected DigitalEntity(Vector2 position, Vector2 scale) : base(position, scale)
    {
    }

    protected DigitalEntity(Vector2 position) : base(position)
    {
    }

    protected DigitalEntity(Vector2 position, Vector2 scale, float rotation, uint renderId) : base(position, scale, rotation, renderId)
    {
    }

    protected DigitalEntity(Vector2 position, Vector2 scale, uint renderId) : base(position, scale, renderId)
    {
    }
    
    public uint GetInputSlotIdAt(int index)
    {
        return InputSlots[index];
    }
    
    public uint GetOutputSlotIdAt(int index)
    {
        return OutputSlots[index];
    }
    
}

public abstract class DraggableDigitalEntity: DigitalEntity, IDraggableSceneEntity
{
    protected DraggableDigitalEntity(Transform transform, uint renderId) : base(transform, renderId)
    {
        SceneState.Instance.DraggableEntities.Add(RenderId, this);
    }

    protected DraggableDigitalEntity(Vector2 position, Vector2 scale) : base(position, scale)
    {
        SceneState.Instance.DraggableEntities.Add(RenderId, this);
    }

    protected DraggableDigitalEntity(Vector2 position) : base(position)
    {
        SceneState.Instance.DraggableEntities.Add(RenderId, this);
    }

    protected DraggableDigitalEntity(Vector2 position, Vector2 scale, float rotation, uint renderId) : base(position, scale, rotation, renderId)
    {
        SceneState.Instance.DraggableEntities.Add(RenderId, this);
    }

    protected DraggableDigitalEntity(Vector2 position, Vector2 scale, uint renderId) : base(position, scale, renderId)
    {
        SceneState.Instance.DraggableEntities.Add(RenderId, this);
    }

    public virtual void UpdatePosition(Vector2 pos)
    {
        Position = pos;
        foreach (var slot in InputSlots)
        {
            SceneState.Instance.GetSlotEntityByRenderId(slot).UpdateParentPos(pos);
        }
        
        foreach (var slot in OutputSlots)
        {
            SceneState.Instance.GetSlotEntityByRenderId(slot).UpdateParentPos(pos);
        }
    }

    public virtual Vector2 GetOffset(Vector2 point)
    {
        return point - Position;
    }
}