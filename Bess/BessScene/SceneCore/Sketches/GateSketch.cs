using System.Numerics;
using BessScene.SceneCore.State;
using BessScene.SceneCore.Entities;
using SkiaSharp;
using SkiaExtensions = SkiaSharp.SkiaExtensions;

namespace BessScene.SceneCore.Sketches;

public class GateSketch: DraggableDigitalEntity
{
    private readonly int _inputCount;
    private readonly int _outputCount;
    private readonly string _name;

    private const float Padding = 6;
    private const float Width = 120;
    private const float HeaderHeight = 14;
    private const float RowPadding = 2;
    private const float RowGap = 8;
    private const float BorderRadius = 4;
    private const float SlotsTopMargin = 4;
    private static readonly float SlotSize = SlotSketch.SlotSize;

    private readonly SKColor _color;
    
    public GateSketch(string name, int inputCount, int outputCount) : base(SceneState.Instance.Camera.Center)
    {
        _name = name;
        _inputCount = inputCount;
        _outputCount = outputCount;
        
        Scale = new Vector2(Width, GetHeight());
        
        InputSlots = GenerateSlots(inputCount);
        OutputSlots = GenerateSlots(outputCount, false);

        _color = SkiaExtensions.GenerateRandomColor();

        UpdatePosition(Position - (Scale / 2));
        
        SceneState.Instance.AddEntity(this);
    }

    public override void Render()
    {
        var borderColor = IsSelected ? SKColors.White : SKColors.Transparent;
        
        SkRenderer.DrawMicaRoundRect(SkPosition, SkScale, BorderRadius, GetRIdColor(), _color, borderColor, 0.5f);
        SkRenderer.DrawMicaRoundRect(SkPosition, HeaderSize, new Vector4(BorderRadius, BorderRadius, 0, 0));
        
        SkRenderer.DrawText(_name, GetHeaderTextPosition(), SKColors.White, 10);
    }
    
    public override void Remove()
    {
        foreach (var slot in InputSlots)
        {
            SceneState.Instance.GetSlotEntityByRenderId(slot).Remove();
        }
        
        foreach (var slot in OutputSlots)
        {
            SceneState.Instance.GetSlotEntityByRenderId(slot).Remove();
        }

        SceneState.Instance.RemoveEntity(this);
    }
    
    private float GetHeight()
    {
        var n = Math.Max(_inputCount, _outputCount);
        return HeaderHeight + RowHeight * n + RowGap * (n - 1) + Padding + Padding + SlotsTopMargin;
    }
    
    private static SKSize HeaderSize => new(Width, HeaderHeight);
    
    private static float RowHeight => SlotSize + RowPadding + RowPadding;
    
    private SKPoint GetHeaderTextPosition()
    {
        var pos = Position + new Vector2(Padding, HeaderHeight / 2);
        return new SKPoint(pos.X, pos.Y);
    }

    private List<uint> GenerateSlots(int count, bool inputSlots = true)
    {
        var x = inputSlots ? Padding + SlotSize / 2 : Width - Padding - SlotSize / 2;
        var y = HeaderHeight + RowPadding + SlotSize + SlotsTopMargin;
        var slots = new List<uint>();
        var labelCounter = 0;
        var labelChar = inputSlots ? 'X' : 'Y';
        for(var i = 0; i < count; i++)
        {
            var label = $"{labelChar}{labelCounter++}"; 
            var dir = inputSlots ? SlotEntity.LabelLocation.Right : SlotEntity.LabelLocation.Left;
            var slot = new SlotSketch(label, new Vector2(x, y), RenderId, i, inputSlots, dir)
            {
                ParentPos = Position
            };
            slots.Add(slot.RenderId);
            y += RowHeight + RowGap;
        }
        return slots;
    }
    
    public override void Update()
    {
        var changeEntries = SceneState.Instance.SceneChangeEntries;
        
        if(!changeEntries.TryGetValue(RenderId, out var changes)) return;

        foreach (var change in changes)
        {
            if (change.IsInputSlot)
            {
                var slot = SceneState.Instance.GetSlotEntityByRenderId(InputSlots[change.SlotIndex]);
                slot.High = change.IsHigh;
            }
            else
            {
                var slot = SceneState.Instance.GetSlotEntityByRenderId(OutputSlots[change.SlotIndex]);
                slot.High = change.IsHigh;
            }
        }
        
    }
}