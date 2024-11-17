using System.Numerics;

namespace BessScene.SceneCore.Entities;

public class DigitalInput: DraggableSceneEntity
{
    
    private uint _outputSlotId 
    
    public DigitalInput() : base(SceneState.Instance.Camera.Center)
    {
    }

    public override void Render()
    {
        
    }

    public override void Remove()
    {
    }

    public override void Update()
    {
    }

    public override void UpdatePosition(Vector2 position)
    {
    }

    public override Vector2 GetOffset(Vector2 point)
    {
        return point - Position;
    }
}