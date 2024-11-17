using System.Numerics;
using BessScene.SceneCore.Entities;
using SkiaSharp;

namespace BessScene.SceneCore.Sketches;

public class DigitalInputSketch: DigitalInputEntity
{
    public override void Render()
    {
        var color = IsHigh ? SKColors.ForestGreen : SKColors.LightCoral;

        var borderColor = IsSelected ? SKColors.White : SKColors.Transparent;
        
        SkRenderer.DrawMicaRoundRect(SkPosition, SkScale, 4, GetRIdColor(), color, borderColor, 0.5f);
        
        borderColor = IsHigh ? color : SKColors.Transparent;
        color = IsHigh ? SKColors.Transparent : color;
        SkRenderer.DrawMicaRoundRect(ButtonPos.ToSkPoint(), 
            ButtonSize.ToSkSize(), 4, 
            RIdToColor(ButtonRenderId), color, borderColor, 0.5f);

        var text = IsHigh ? "1" : "0";
        
        SkRenderer.DrawText(text, TextPos.ToSkPoint(), SKColors.Azure, 10);
    }

    private Vector2 ButtonPos => Position + ButtonOffset;
    
    private Vector2 ButtonOffset => new (Padding, (RenderHeight - ButtonSize.Y) / 2.0f);
    
    private Vector2 ButtonSize => (Scale - new Vector2(Padding) - new Vector2(2)) with {X = Scale.X * 0.4f};
    
    private Vector2 TextOffset => (ButtonSize) / 2.0f;
    
    private Vector2 TextPos => ButtonPos + TextOffset;
}