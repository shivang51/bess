using System.Numerics;
using SkiaSharp;

namespace BessScene.SceneCore.Sketches;

public class GateSketch: SceneEntity
{
    private readonly int _inputCount;
    private readonly int _outputCount;
    private readonly string _name;
    
    private readonly List<SlotSketch> _inputSlots;
    private readonly List<SlotSketch> _outputSlots;

    private const float Padding = 4;
    private const float Width = 100;
    private const float HeaderHeight = 20;
    private const float RowPadding = 4;
    private const float RowGap = 2;
    private const float BorderRadius = 4;
    private const float SlotSize = SlotSketch.SlotSize;
    
    public GateSketch(string name, int inputCount, int outputCount) : base(new Vector2(20, 20))
    {
        _name = name;
        _inputCount = inputCount;
        _outputCount = outputCount;
        
        Scale = new Vector2(Width, GetHeight());
        
        _inputSlots = GenerateSlots(inputCount);
        _outputSlots = GenerateSlots(outputCount, false);
    }

    public override void Render()
    {
        var borderColor = IsHovered ? SKColors.RosyBrown : SKColors.White;
        SkRenderer.DrawRoundRect(SkPosition, SkScale, BorderRadius, SKColors.Gray, GetRIdColor(), borderColor, 2);
        SkRenderer.DrawRoundRect(SkPosition, HeaderSize, new Vector4(BorderRadius, BorderRadius, 0, 0), SKColors.DarkGray, GetRIdColor());
        
        SkRenderer.DrawText(_name, GetHeaderTextPosition(), SKColors.White, 12);
        
        foreach (var slot in _inputSlots)
        {
            slot.Render();
        }
        
        foreach (var slot in _outputSlots)
        {
            slot.Render();
        }
    }
    
    private float GetHeight()
    {
        var n = Math.Max(_inputCount, _outputCount);
        return HeaderHeight + RowHeight * n + RowGap * (n - 1) + Padding;
    }
    
    private static SKSize HeaderSize => new(Width, HeaderHeight);

    private Vector2 RightCornerPos => new(Position.X + Width, Position.Y);
    
    private static float RowHeight => SlotSize + RowPadding + RowPadding;
    
    private SKPoint GetHeaderTextPosition()
    {
        var pos = Position + new Vector2(4, HeaderHeight / 2);
        return new SKPoint(pos.X, pos.Y);
    }

    private List<SlotSketch> GenerateSlots(int count, bool inputSlots = true)
    {
        var x = inputSlots ? Position.X + Padding + SlotSize : RightCornerPos.X - Padding - SlotSize;
        var y = Position.Y + HeaderHeight + RowGap + RowHeight / 2;
        var slots = new List<SlotSketch>();
        while (count-- > 0)
        {
            var slot = new SlotSketch(new Vector2(x, y));
            slots.Add(slot);
            y += RowHeight + RowGap;
        }
        return slots;
    }
    
}