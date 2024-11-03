using System.Numerics;
using SkiaSharp;

namespace BessScene.SceneCore.Sketches;

public class SlotSketch: SceneEntity
{
    public enum LabelLocation
    {
        Left,
        Right,
        Top,
        Bottom
    }
    
    public const float SlotSize = 10;
    public const float LabelGap = 4;
    public const float FontSize = 10;
    
    public bool High { get; set; }
    
    public string Name { get; set; } = "";

    public LabelLocation LabelLoc { get; set; } = LabelLocation.Right;
    
    private readonly Vector2 _labelOffset;
    
    public SlotSketch(Vector2 pos): base(pos){}

    public SlotSketch(string name, Vector2 pos, LabelLocation labelLocation = LabelLocation.Right) : base(pos)
    {
        Name = name;
        LabelLoc = labelLocation;

        
        // Calculating label offset
        float xOffset = 0;
        float yOffset = 0;
        
        switch (LabelLoc)
        {
            case LabelLocation.Left:
                xOffset = -(SlotSize / 2) - LabelGap - SkRenderer.CalculateTextWidth(Name, FontSize);
                break;
            case LabelLocation.Right:
                xOffset = (SlotSize / 2) + LabelGap;
                break;
            case LabelLocation.Top:
                yOffset = -(SlotSize / 2) - LabelGap - FontSize;
                break;
            case LabelLocation.Bottom:
                yOffset = (SlotSize / 2) + LabelGap;
                break;
        }
        
        _labelOffset = new Vector2(xOffset, yOffset);
    }


    public override void Render()
    {
        if(!string.IsNullOrEmpty(Name))
            SkRenderer.DrawText(Name, SkPosition + SkLabelOffset, SKColors.White, FontSize);
        SkRenderer.DrawCircle(SkPosition, SlotSize / 2, SKColors.Transparent, GetRIdColor(), SKColors.White, 1);
    }

    private SKPoint SkLabelOffset => new(_labelOffset.X, _labelOffset.Y);
}