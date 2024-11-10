using System.Numerics;

namespace BessScene.SceneCore.State.ShadersCollection;

public class EmptyEntity : SceneEntity
{
    public EmptyEntity(): base(new Vector2(0, 0), new Vector2(0, 0), 0)
    {
    }
    
    public override void Render()
    {
    }
    
    public override void Remove()
    {
    }
}