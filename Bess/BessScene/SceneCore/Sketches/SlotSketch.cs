using System.Numerics;
using System.Reflection.Metadata;
using BessScene.SceneCore.State;
using BessScene.SceneCore.Entities;
using SkiaSharp;

namespace BessScene.SceneCore.Sketches;

public class SlotSketch: SlotEntity
{
    public SlotSketch(string name, Vector2 pos, uint parentId,  int index, bool isInput,LabelLocation labelLocation = LabelLocation.Right) : 
        base(name, pos, parentId, index, isInput, labelLocation)
    {
    }

    public override void Render()
    {
        float borderWeight = 1;
        var radius = SlotDiameter / 2;
        if (IsHovered)
        {
            borderWeight = (float)1.5;
            radius = SlotDiameter / (float)2.1;
        }

        var pos = ParentPos.ToSkPoint() + SkPosition;
        
        if(!string.IsNullOrEmpty(Name))
            SkRenderer.DrawText(Name, pos + SkLabelOffset, SKColors.White, FontSize);

        var bgColor = SKColors.IndianRed.WithAlpha(200);
        if (High) bgColor = SKColors.LimeGreen.WithAlpha(200);
        SkRenderer.DrawCircle(pos, radius, bgColor, GetRIdColor(), SKColors.White, borderWeight);
    }
}