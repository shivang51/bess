using System.Numerics;
using SkiaSharp;

namespace BessScene.SceneCore.State.SceneCore.Entities.Sketches;

public class SlotSketch: SceneEntity
{
    public enum LabelLocation
    {
        Left,
        Right,
        Top,
        Bottom
    }
    
    public const float SlotSize = 8;
    public const float LabelGap = 4;
    public const float FontSize = 10;
    
    public bool High { get; set; }
    
    private uint _parentRenderId;
    private SKPoint _parentPos = new();
    
    public string Name { get; set; } = "";

    public LabelLocation LabelLoc { get; set; } = LabelLocation.Right;
    
    private readonly Vector2 _labelOffset;

    public SlotSketch(string name, Vector2 pos, uint parentId, LabelLocation labelLocation = LabelLocation.Right) : base(pos)
    {
        Name = name;
        LabelLoc = labelLocation;

        _parentRenderId = parentId;
        
        // Calculating label offset
        float xOffset = 0;
        float yOffset = (FontSize - SlotSize) / 2;
        
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

    public void Render(SKPoint parentPos)
    {
        _parentPos = parentPos;
        Render();
    }
    
    public override void Render()
    {
        float borderWeight = 1;
        var radius = SlotSize / 2;
        if (IsHovered)
        {
            borderWeight = (float)1.5;
            radius = SlotSize / (float)2.1;
        }

        var pos = _parentPos + SkPosition;
        
        if(!string.IsNullOrEmpty(Name))
            SkRenderer.DrawText(Name, pos + SkLabelOffset, SKColors.White, FontSize);
        SkRenderer.DrawCircle(pos, radius, SKColors.Transparent, GetRIdColor(), SKColors.White, borderWeight);
    }

    private SKPoint SkLabelOffset => new(_labelOffset.X, _labelOffset.Y);
}