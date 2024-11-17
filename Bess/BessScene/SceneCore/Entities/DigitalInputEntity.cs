using System.Numerics;
using BessScene.SceneCore.Sketches;
using BessScene.SceneCore.State;
using SkiaSharp;

namespace BessScene.SceneCore.Entities;

public abstract class DigitalInputEntity: DraggableDigitalEntity
{

    protected const float Width = 40;
    protected const float Height = 12;
    protected const float Padding = 4;

    protected bool IsHigh { get; set; } = false;
    
    public uint ButtonRenderId { get; }

    protected DigitalInputEntity() : base(SceneState.Instance.Camera.Center)
    {
        Scale = new Vector2(RenderWidth, RenderHeight);
        
        var slot = new SlotSketch("", new Vector2(RenderWidth - Padding * 2, RenderHeight / 2), RenderId, 0, false)
        {
            ParentPos = Position
        };

        OutputSlots.Add(slot.RenderId);

        ButtonRenderId = NextRenderId;
        
        SceneState.Instance.AddEntity(this);
        SceneState.Instance.ButtonRenderIds.Add(ButtonRenderId, RenderId);
    }

    private uint OutputSlotId => OutputSlots[0];
    
    protected static float RenderWidth => Width + (Padding * 2);
    
    protected static float RenderHeight => Height + (Padding * 2);
    
    public override void Remove()
    {
        SceneState.Instance.GetSlotEntityByRenderId(OutputSlotId).Remove();
        SceneState.Instance.RemoveEntity(this);
    }
    
    public override void Update()
    {
        var changeEntries = SceneState.Instance.SceneChangeEntries;
        if(!changeEntries.TryGetValue(RenderId, out var changes)) return;
        
        foreach (var change in changes)
        {
            if (change.IsInputSlot) return;
            var slot = SceneState.Instance.GetSlotEntityByRenderId(OutputSlotId);
            slot.High = change.IsHigh;
        }
    }

    public void ToggleState()
    {
        IsHigh = !IsHigh;
        SceneState.Instance.ChangedInputIds.Add(RenderId, IsHigh);
    }
}