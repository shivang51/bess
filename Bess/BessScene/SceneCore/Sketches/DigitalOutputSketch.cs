using System.Numerics;
using BessScene.SceneCore.Entities;
using SkiaSharp;

namespace BessScene.SceneCore.Sketches;

public class DigitalOutputSketch: DigitalOutputEntity
{
    public override void Render()
    {
        var color = IsHigh ? SKColors.ForestGreen : SKColors.Maroon;
        var borderColor = IsSelected ? SKColors.WhiteSmoke : SKColors.Transparent;
        SkRenderer.DrawMicaRoundRect(SkPosition, SkScale, 4, GetRIdColor(), color, borderColor, 0.5f);
        
        var text = IsHigh ? "1" : "0";
        SkRenderer.DrawText(text, (Position + TextOffset).ToSkPoint(), SKColors.WhiteSmoke, 10);
    }
    
    public Vector2 TextOffset => new Vector2(RenderWidth / 2, RenderHeight / 2);

}