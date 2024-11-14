using System.Numerics;
using BessScene.SceneCore.State;
using SkiaSharp;

namespace BessScene.SceneCore.Entities;

public class ConnectionSegment: DraggableSceneEntity
{
    // these points are relative to the start slot position
    public Vector2 StartPos { get; set; }
    
    public Vector2 EndPos { get; set; }
    
    public uint ParentId { get; init; }

    public ConnectionSegment? PreviousSegment { get; set; } = null;
    
    public ConnectionSegment? NextSegment { get; set; } = null;
    
    
    public ConnectionSegment(Vector2 startPos, Vector2 endPos, uint parentId) : base(new Vector2(), new Vector2())
    {
        StartPos = startPos;
        EndPos = endPos;
        ParentId = parentId;
        
        SceneState.Instance.ConnectionSegments.Add(RenderId, this);
    }
    
    public bool IsPointInside(Vector2 point)
    {
        return point.X >= StartPos.X && point.X <= EndPos.X && point.Y >= StartPos.Y && point.Y <= EndPos.Y;
    }
    
    public bool IsStartOrEnd(Vector2 point)
    {
        return point == StartPos || point == EndPos;
    }

    public Vector2 AbsStart(Vector2 pos) => pos + StartPos;
    public Vector2 AbsEnd(Vector2 pos) => pos + EndPos;

    public override void Render()
    {
    }

    public override void Remove()
    {
    }

    public override void Update()
    {
    }

    public bool IsHovered => SceneState.Instance.SecondaryHoveredEntityId == RenderId;
    
    public override void UpdatePosition(Vector2 position)
    {
        var dSe = EndPos - StartPos;
        
        if(dSe.X == 0) position = position with {Y = StartPos.Y};
        else if(dSe.Y == 0) position = position with {X = StartPos.X};
        
        var dPos = position - StartPos;
        StartPos = position;
        EndPos += dPos;
        
        if (PreviousSegment != null)
        {
            PreviousSegment.EndPos = StartPos;
        }
        
        if (NextSegment != null)
        {
            NextSegment.StartPos = EndPos;
        }
    }
    
    public override Vector2 GetOffset(Vector2 point)
    {
        return point - StartPos;
    }

    public void Render(Vector2 startPos, SKColor color, float weight)
    {
        if (IsHovered)
        {
            weight += 0.5f;
            color = SKColors.Beige;
        }
        SkRenderer.DrawLine((StartPos + startPos).ToSkPoint(), (EndPos + startPos).ToSkPoint(), color, weight, GetRIdColor());
    }
}

public abstract class ConnectionEntity: SceneEntity
{
    public uint StartSlotId { get; set; }
    public uint EndSlotId { get; set; }
    
    protected readonly List<ConnectionSegment> ConnectionSegments = new();
    
    protected ConnectionEntity(uint startSlotId, uint endSlotId) : base(new Vector2(), new Vector2())
    {
        StartSlotId = startSlotId;
        EndSlotId = endSlotId;
        
        SceneState.Instance.AddConnectionEntity(this);
        
        SceneState.Instance.AddToConnectionMap(StartSlotId, RenderId);
        SceneState.Instance.AddToConnectionMap(EndSlotId, RenderId);
        
        FillDefaultConnectionSegments();
    }
    
    public override void Remove()
    {
        SceneState.Instance.RemoveFromConnectionMap(StartSlotId, RenderId);
        SceneState.Instance.RemoveFromConnectionMap(EndSlotId, RenderId);
        
        SceneState.Instance.RemoveConnectionEntity(this);
    }
    
    private void FillDefaultConnectionSegments()
    {
        var slot1 = SceneState.Instance.GetSlotEntityByRenderId(StartSlotId);
        var startPos = slot1.AbsPosition;
        var endPos = SceneState.Instance.GetSlotEntityByRenderId(EndSlotId).AbsPosition;
        
        
        var dPos = endPos - startPos;
        var midPointX = dPos.X / 2;
        
        ConnectionSegments.Add(new ConnectionSegment(new Vector2(0), new Vector2(midPointX, 0), RenderId));
        ConnectionSegments.Add(new ConnectionSegment(new Vector2(midPointX, 0), new Vector2(midPointX, dPos.Y), RenderId));
        ConnectionSegments.Add(new ConnectionSegment(new Vector2(midPointX, dPos.Y), dPos, RenderId));
        
        ConnectionSegment? prevSegment = null;

        for(var i = 0; i < ConnectionSegments.Count; i++)
        {
            var seg = ConnectionSegments[i];
            var nextSegment = (ConnectionSegments.Count != i + 1) ? ConnectionSegments[i + 1] : null;
            
            seg.PreviousSegment = prevSegment;
            seg.NextSegment = nextSegment;
            
            prevSegment = seg;
        }
    }
    
    public override void Update()
    {
        var slot1 = SceneState.Instance.GetSlotEntityByRenderId(StartSlotId);
        var startPos = slot1.AbsPosition;
        var endPos = SceneState.Instance.GetSlotEntityByRenderId(EndSlotId).AbsPosition;
        
        
        if (ConnectionSegments.First().AbsStart(startPos) != startPos)
        {
            var absStart = ConnectionSegments.First().AbsStart(startPos);
            var newSeg = new ConnectionSegment(Vector2.Zero, absStart - startPos, RenderId);
            ConnectionSegments.Insert(0, newSeg);
            ConnectionSegments[1].PreviousSegment = newSeg;
            newSeg.NextSegment = ConnectionSegments[1];
        }
        
        if(ConnectionSegments.Last().AbsEnd(startPos) != endPos)
        {
            var absEnd = ConnectionSegments.Last().AbsEnd(startPos);
            var newSeg = new ConnectionSegment(absEnd - startPos, endPos - startPos, RenderId);
            ConnectionSegments.Add(newSeg);
            ConnectionSegments[^2].NextSegment = newSeg;
            newSeg.PreviousSegment = ConnectionSegments[^2];
        }
    }
    
    public void UpdateParentPos(Vector2 _)
    {
        Console.WriteLine("updating parent");
        var slot1 = SceneState.Instance.GetSlotEntityByRenderId(StartSlotId);
        var startPos = slot1.AbsPosition;
        var endPos = SceneState.Instance.GetSlotEntityByRenderId(EndSlotId).AbsPosition;
        
        // [TODO] (Shivang) : Fix end pos calculation
    }
    
}