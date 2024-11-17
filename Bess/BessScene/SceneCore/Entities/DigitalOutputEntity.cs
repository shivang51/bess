using System.Numerics;
using BessScene.SceneCore.Sketches;
using BessScene.SceneCore.State;

namespace BessScene.SceneCore.Entities;

public abstract class DigitalOutputEntity: DraggableDigitalEntity
{
    protected const float Width = 24;
    protected const float Height = 12;
    protected const float Padding = 4;


    protected DigitalOutputEntity() : base(SceneState.Instance.Camera.Center)
    {
        Scale = new Vector2(RenderWidth, RenderHeight);
        
        var slot = new SlotSketch("", new Vector2(Padding * 2, RenderHeight / 2), RenderId, 0, true)
        {
            ParentPos = Position
        };

        InputSlots.Add(slot.RenderId);
        
        SceneState.Instance.AddEntity(this);
    }
    
    
    protected bool IsHigh => SceneState.Instance.GetSlotEntityByRenderId(InputSlots[0]).High;
    
    protected static float RenderWidth => Width + (Padding * 2);
    
    protected static float RenderHeight => Height + (Padding * 2);
    
    private uint InputSlotId => InputSlots[0];
    
    public override void Update()
    {
        var changeEntries = SceneState.Instance.SceneChangeEntries;
        if(!changeEntries.TryGetValue(RenderId, out var changes)) return;
        
        foreach (var change in changes)
        {
            if (!change.IsInputSlot) return;
            var slot = SceneState.Instance.GetSlotEntityByRenderId(InputSlotId);
            slot.High = change.IsHigh;
        }
    }
    
    public override void Remove()
    {
        SceneState.Instance.GetSlotEntityByRenderId(InputSlots[0]).Remove();
        SceneState.Instance.Entities.Remove(RenderId);
    }
}