using System.Numerics;
using BessScene.SceneCore.Entities;
using BessScene.SceneCore.ShadersCollection;
using SkiaSharp;

namespace BessScene.SceneCore.Sketches;

public class ConnectionSketch: ConnectionEntity
{
    public ConnectionSketch(uint startSlotId, uint endSlotId) : base(startSlotId, endSlotId)
    {
    }

    public override void Render()
    {
        var startPos = SceneState.Instance.GetSlotEntityByRenderId(StartSlotId).AbsPosition;
        var endPos = SceneState.Instance.GetSlotEntityByRenderId(EndSlotId).AbsPosition;
        
        SkRenderer.DrawCubicBezier(startPos.ToSkPoint(), endPos.ToSkPoint(), SKColors.Olive, 1.5f, GetRIdColor());
    }
}