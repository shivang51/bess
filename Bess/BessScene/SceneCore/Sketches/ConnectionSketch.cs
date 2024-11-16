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

    private void RenderCurve(){
        var slot1 = SceneState.Instance.GetSlotEntityByRenderId(StartSlotId);
        var startPos = slot1.AbsPosition;
        var endPos = SceneState.Instance.GetSlotEntityByRenderId(EndSlotId).AbsPosition;

        var color = slot1.High ? SKColors.DarkOliveGreen : SKColors.IndianRed.WithAlpha(150);
        SkRenderer.DrawCubicBezier(startPos.ToSkPoint(), endPos.ToSkPoint(), color, Weight, GetRIdColor());
    }

    private void RenderLine()
    {
        var slot1 = SceneState.Instance.GetSlotEntityByRenderId(StartSlotId);
        var startPos = slot1.AbsPosition;

        var color = slot1.High ? SKColors.DarkOliveGreen : SKColors.IndianRed.WithAlpha(150);

        foreach (var segment in ConnectionSegments)
        {
            segment.Render(startPos, Weight);
        }

        var points = ConnectionSegments.Select(el => el.AbsStart(startPos).ToSkPoint()).ToList();
        
        points.Add(SceneState.Instance.GetSlotEntityByRenderId(EndSlotId).AbsPosition.ToSkPoint());
        
        SkRenderer.DrawLinePath(points.ToArray(), color, Weight);
    }
    
    public override void Render()
    {
        RenderLine();
    }
    
    private float Weight => IsHovered ? 2.0f : 1.5f;
}