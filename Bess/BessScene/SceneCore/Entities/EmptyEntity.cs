using System.Numerics;

namespace BessScene.SceneCore.State.SceneCore.Entities;

public class EmptyEntity : SceneEntity
{
    public EmptyEntity(): base(new Vector2(0, 0), new Vector2(0, 0), 0)
    {
    }
    
    public override void Render()
    {
    }
}