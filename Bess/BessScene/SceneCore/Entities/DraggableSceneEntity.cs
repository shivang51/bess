using System.Numerics;

namespace BessScene.SceneCore.Entities;

public abstract class DraggableSceneEntity: SceneEntity
{
    protected DraggableSceneEntity(Transform transform, uint renderId) : base(transform, renderId)
    {
    }

    protected DraggableSceneEntity(Vector2 position, Vector2 scale) : base(position, scale)
    {
    }

    protected DraggableSceneEntity(Vector2 position) : base(position)
    {
    }

    protected DraggableSceneEntity(Vector2 position, Vector2 scale, float rotation, uint renderId) : base(position, scale, rotation, renderId)
    {
    }

    protected DraggableSceneEntity(Vector2 position, Vector2 scale, uint renderId) : base(position, scale, renderId)
    {
    }

    public abstract void UpdatePosition(Vector2 position);
}