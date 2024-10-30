using System.Numerics;
using SkiaSharp;

namespace BessScene.SceneCore;

public class DummyEntity: SceneEntity
{
    public DummyEntity() : base(new Vector2(0, 0), new Vector2(200, 100), 1)
    {
    }

    public override void Render()
    {
        // SkRenderer.DrawCircle(SkPosition, 100, SKColors.Red, GetRIdColor(), SKColors.Azure, 2);
        SkRenderer.DrawRoundRect(SkPosition, SkScale, 8, SKColors.Red, GetRIdColor(), SKColors.Azure, 2);
    }
}