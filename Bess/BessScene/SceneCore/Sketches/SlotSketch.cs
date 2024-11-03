using System.Numerics;
using SkiaSharp;

namespace BessScene.SceneCore.Sketches;

public class SlotSketch: SceneEntity
{
    public const float SlotSize = 10;
    
    public bool High { get; set; }
    
    public SlotSketch(Vector2 pos): base(pos){}


    public override void Render()
    {
        SkRenderer.DrawCircle(SkPosition, SlotSize / 2, SKColors.Transparent, GetRIdColor(), SKColors.White, 1);
    }
}