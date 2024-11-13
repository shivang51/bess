using System.Drawing;
using System.Numerics;
using BessScene.SceneCore.Entities;
using SkiaSharp;

namespace BessScene.SceneCore.Sketches;

public class ConnectionSketch: ConnectionEntity
{
    public ConnectionSketch(uint startSlotId, uint endSlotId) : base(startSlotId, endSlotId)
    {
    }

    public override void Render()
    {
        var slot1 = SceneState.Instance.GetSlotEntityByRenderId(StartSlotId);
        var startPos = slot1.AbsPosition;
        var endPos = SceneState.Instance.GetSlotEntityByRenderId(EndSlotId).AbsPosition;

        var color = slot1.High ? SKColors.DarkOliveGreen : SKColors.IndianRed.WithAlpha(150);
        SkRenderer.DrawCubicBezier(startPos.ToSkPoint(), endPos.ToSkPoint(), color, 1.5f, GetRIdColor());
    }
    
    public override void Update()
    {
    }
}