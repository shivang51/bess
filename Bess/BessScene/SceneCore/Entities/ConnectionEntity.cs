using System.Numerics;
using BessScene.SceneCore.Sketches;
using BessScene.SceneCore.State;
using BessScene.SceneCore.State.State;
using SkiaSharp;

namespace BessScene.SceneCore.Entities;

public class ConnectionSegment: DraggableDigitalEntity
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
        else // first entity
        {
            var newSeg = new ConnectionSegment(Vector2.Zero, StartPos, ParentId);
            var parentEnt = SceneState.Instance.GetConnectionEntity(ParentId);
            
            parentEnt.ConnectionSegments.Insert(0, newSeg);
            parentEnt.ConnectionSegments[1].PreviousSegment = newSeg;
            newSeg.NextSegment = parentEnt.ConnectionSegments[1];
        }
        
        if (NextSegment != null)
        {
            NextSegment.StartPos = EndPos;
        }
        else // last entity
        {
            var parentEnt = SceneState.Instance.GetConnectionEntity(ParentId);
            var startSlot = SceneState.Instance.GetSlotEntityByRenderId(parentEnt.StartSlotId);
            var endSlot = SceneState.Instance.GetSlotEntityByRenderId(parentEnt.EndSlotId);
            var newSeg = new ConnectionSegment(EndPos, endSlot.AbsPosition - startSlot.AbsPosition, ParentId);
            parentEnt.ConnectionSegments.Add(newSeg);
            parentEnt.ConnectionSegments[^2].NextSegment = newSeg;
            newSeg.PreviousSegment = parentEnt.ConnectionSegments[^2];
        }
    }
    
    public override Vector2 GetOffset(Vector2 point)
    {
        return point - StartPos;
    }

    public void Render(Vector2 startPos, float weight)
    {
        SkRenderer.DrawLineId((StartPos + startPos).ToSkPoint(), (EndPos + startPos).ToSkPoint(), weight, GetRIdColor());
    }
}

public abstract class ConnectionEntity: SceneEntity
{
    public uint StartSlotId { get; set; }
    public uint EndSlotId { get; set; }
    
    public readonly List<ConnectionSegment> ConnectionSegments = new();
    
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
    }
    
    public void UpdateParentPos(Vector2 _)
    {
        var slot1 = SceneState.Instance.GetSlotEntityByRenderId(StartSlotId);
        var startPos = slot1.AbsPosition;
        var endPos = SceneState.Instance.GetSlotEntityByRenderId(EndSlotId).AbsPosition;
        
        // [TODO] (Shivang) : Fix end pos calculation
        // -- fixed for horizontal end connection

        var lastSeg = ConnectionSegments.Last();
        var currentDif = lastSeg.EndPos - lastSeg.StartPos;

        var dPos = endPos - startPos;
        lastSeg.EndPos = dPos;
        lastSeg.StartPos = dPos - currentDif;
        currentDif = lastSeg.PreviousSegment.EndPos - lastSeg.PreviousSegment.StartPos;
        lastSeg.PreviousSegment.EndPos = lastSeg.StartPos;

        var pos = lastSeg.PreviousSegment.EndPos - currentDif;

        currentDif = lastSeg.EndPos - lastSeg.StartPos;
        if (currentDif.X == 0)
        {
            lastSeg.PreviousSegment.StartPos = lastSeg.PreviousSegment.StartPos with { Y = pos.Y };
        }
        else if (currentDif.Y == 0)
        {
            lastSeg.PreviousSegment.StartPos = lastSeg.PreviousSegment.StartPos with { X = pos.X };
        }
            
        lastSeg.PreviousSegment.PreviousSegment.EndPos = lastSeg.PreviousSegment.StartPos;
    }
    
}