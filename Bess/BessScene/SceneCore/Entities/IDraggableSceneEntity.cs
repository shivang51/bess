using System.Numerics;

namespace BessScene.SceneCore.Entities;

public interface IDraggableSceneEntity
{
    public abstract void UpdatePosition(Vector2 position);

    public abstract Vector2 GetOffset(Vector2 point);
}