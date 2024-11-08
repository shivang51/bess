using System.Numerics;
using BessScene.SceneCore.Entities;
using BessScene.SceneCore.ShadersCollection;
using SkiaSharp;

namespace BessScene.SceneCore.Sketches;

public class GateSketch: SceneEntity
{
    private readonly int _inputCount;
    private readonly int _outputCount;
    private readonly string _name;
    
    private readonly List<uint> _inputSlots;
    private readonly List<uint> _outputSlots;

    private const float Padding = 6;
    private const float Width = 120;
    private const float HeaderHeight = 14;
    private const float RowPadding = 2;
    private const float RowGap = 8;
    private const float BorderRadius = 4;
    private const float SlotsTopMargin = 4;
    private static readonly float SlotSize = SlotSketch.SlotSize;

    private readonly SKColor _color;
    
    public GateSketch(string name, int inputCount, int outputCount) : base(new Vector2(20, 20))
    {
        _name = name;
        _inputCount = inputCount;
        _outputCount = outputCount;
        
        Scale = new Vector2(Width, GetHeight());
        
        _inputSlots = GenerateSlots(inputCount);
        _outputSlots = GenerateSlots(outputCount, false);

        _color = SkiaExtensions.GenerateRandomColor();
    }

    public override void Render()
    {
        SkRenderer.DrawMicaRoundRect(SkPosition, SkScale, BorderRadius, GetRIdColor(), _color);
        SkRenderer.DrawMicaRoundRect(SkPosition, HeaderSize, new Vector4(BorderRadius, BorderRadius, 0, 0));
        
        SkRenderer.DrawText(_name, GetHeaderTextPosition(), SKColors.White, 10);
    }

    public void UpdatePos(Vector2 pos)
    {
        Position = pos;
        foreach (var slot in _inputSlots)
        {
            SceneState.Instance.GetSlotEntityByRenderId(slot).ParentPos = pos;
        }
        
        foreach (var slot in _outputSlots)
        {
            SceneState.Instance.GetSlotEntityByRenderId(slot).ParentPos = pos;
        }
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
        while (count-- > 0)
        {
            var label = $"{labelChar}{labelCounter++}"; 
            var dir = inputSlots ? SlotEntity.LabelLocation.Right : SlotEntity.LabelLocation.Left;
            var slot = new SlotSketch(label, new Vector2(x, y), RenderId, dir)
            {
                ParentPos = Position
            };
            slots.Add(slot.RenderId);
            y += RowHeight + RowGap;
        }
        return slots;
    }
    
}