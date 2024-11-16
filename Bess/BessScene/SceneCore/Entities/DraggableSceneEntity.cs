using System.Numerics;

namespace BessScene.SceneCore.Entities;

public abstract class DraggableSceneEntity: SceneEntity
{
    protected DraggableSceneEntity(Transform transform, uint renderId) : base(transform, renderId)
    {
        SceneState.Instance.DraggableEntities.Add(RenderId, this);
    }

    protected DraggableSceneEntity(Vector2 position, Vector2 scale) : base(position, scale)
    {
        SceneState.Instance.DraggableEntities.Add(RenderId, this);
    }
    
    protected DraggableSceneEntity(Vector2 position) : base(position)
    {
        SceneState.Instance.DraggableEntities.Add(RenderId, this);
    }

    protected DraggableSceneEntity(Vector2 position, Vector2 scale, float rotation, uint renderId) : base(position, scale, rotation, renderId)
    {
        SceneState.Instance.DraggableEntities.Add(RenderId, this);
    }

    protected DraggableSceneEntity(Vector2 position, Vector2 scale, uint renderId) : base(position, scale, renderId)
    {
        SceneState.Instance.DraggableEntities.Add(RenderId, this);
    }

    public abstract void UpdatePosition(Vector2 position);

    public abstract Vector2 GetOffset(Vector2 point);
}