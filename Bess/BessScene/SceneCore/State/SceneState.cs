using System.Numerics;
using BessScene.SceneCore.Entities;
using BessScene.SceneCore.Sketches;
using BessScene.SceneCore.State;
using SkiaSharp;

namespace BessScene.SceneCore;

public class SceneState
{
    public static SceneState Instance { get; } = new();

    public Vector2 MousePosition = Vector2.Zero;
    
    public SKPoint MousePositionSkPoint => new(MousePosition.X, MousePosition.Y);
    
    public bool IsLeftMousePressed { get; set; }
    
    public bool IsMiddleMousePressed { get; set; }
    
    public DragData DragData { get; private set; } = new();
    
    public ConnectionData ConnectionData { get; private set; } = new();
    
    public Dictionary<uint, List<SceneChangeEntry>> SceneChangeEntries { get; set; } = new();

    public List<NewConnectionEntry> NewConnectionEntries { get; } = new();
    
    public List<NewConnectionEntry> ApprovedConnectionEntries { get; } = new();
    
    public List<Action<uint>> OnSelectedEntityChangedCB { get; } = new();
    
    /// <summary>
    /// Contains connection ids from slot render id
    /// </summary>
    private readonly Dictionary<uint, List<uint>> _connectionMap = new();
    
    public uint HoveredEntityId { get; private set; }
    
    public uint SecondaryHoveredEntityId { get; private set; }
    
    private uint _selectedEntityId = 0;
    
    public Dictionary<uint, ConnectionEntity> ConnectionEntities { get; private set; } = new();
    public Dictionary<uint, SceneEntity> Entities { get; private set; } = new();
    public Dictionary<uint, SlotEntity> SlotEntities { get; private set; } = new();
    public Dictionary<uint, DraggableSceneEntity> DraggableEntities { get; private set; } = new();
    public Dictionary<uint, ConnectionSegment> ConnectionSegments { get; } = new();
    
    public uint SelectedEntityId { 
        get => _selectedEntityId;
        set => SetSelectedEntity(value);
    }
    
    private SceneState()
    {
        Init();   
    }

    private void Init()
    {
        _selectedEntityId = 0;
        Entities.Clear();
        SlotEntities.Clear();
        ConnectionEntities.Clear();
        _connectionMap.Clear();
        
        SceneChangeEntries.Clear();
        NewConnectionEntries.Clear();
        ConnectionSegments.Clear();
        DraggableEntities.Clear();
    }
    
    public bool IsDraggableEntity(uint rid)
    {
        return DraggableEntities.ContainsKey(rid);
    }
    
    public void AddEntity(SceneEntity entity)
    {
        Entities.Add(entity.RenderId, entity);
    }
    
    public void RemoveEntityById(uint renderId)
    {
        var ent = GetEntityOrNull(renderId);
        if (ent == null) throw new InvalidDataException($"Failed to removed an entity with render id {renderId}");
        ent.Remove();
    }
    
    public void RemoveEntity(SceneEntity entity)
    {
        Entities.Remove(entity.RenderId);
    }

    public bool IsParentEntity(uint rid)
    {
        return Entities.ContainsKey(rid);
    }
    
    public bool IsSlotEntity(uint rid)
    {
        return SlotEntities.ContainsKey(rid);
    }
    
    public bool IsConnectionSegment(uint rid)
    {
        return ConnectionSegments.ContainsKey(rid);
    }
    
    public void AddSlotEntity(SlotEntity entity)
    {
        SlotEntities.Add(entity.RenderId, entity);
    }
    
    public void RemoveSlotEntity(SlotEntity entity)
    {
        SlotEntities.Remove(entity.RenderId);
    }
    
    public void AddConnectionEntity(ConnectionEntity entity)
    {
        ConnectionEntities.Add(entity.RenderId, entity);
    }
    
    public ConnectionEntity GetConnectionEntity(uint rid)
    {
        var ent = ConnectionEntities.GetValueOrDefault(rid);
        
        if (ent == null)
        {
            throw new InvalidDataException($"Entity with render id {rid} not found");
        }
        
        return ent;
    }
    
    public void RemoveConnectionEntity(ConnectionEntity entity)
    {
        ConnectionEntities.Remove(entity.RenderId);
    }
    
    public void RemoveFromConnectionMap(uint slotId, uint connId)
    {
        if (_connectionMap.TryGetValue(slotId, out var value))
        {
            value.Remove(connId);
        }
    }
    
    public void AddToConnectionMap(uint slotId, uint connId)
    {
        if (!_connectionMap.TryGetValue(slotId, out var value))
        {
            value = new List<uint>();
            _connectionMap[slotId] = value;
        }

        value.Add(connId);
    }
    
    public List<ConnectionEntity> GetConnectionEntitiesForSlot(uint slotId)
    {
        var connIds = _connectionMap.GetValueOrDefault(slotId);
        return connIds == null ? new List<ConnectionEntity>() : connIds.Select(GetConnectionEntity).ToList();
    }

    public void ClearConnectionMapForSlotId(uint slotId)
    {
        _connectionMap.Remove(slotId);
    }
    
    /// <summary>
    /// Searches all the entities -> Entities + ChildEntities and returns null if not found
    /// </summary>
    /// <param name="rid">Render ID of entity to be searched</param>
    /// <returns></returns>
    private SceneEntity? GetEntityOrNull(uint rid)
    {
        var entity = Entities.GetValueOrDefault(rid);
        entity ??= SlotEntities.GetValueOrDefault(rid);
        entity ??= ConnectionEntities.GetValueOrDefault(rid);
        return entity;
    }
    
    public T GetEntityByRenderId<T>(uint rid) where T: SceneEntity
    {
        var entity = GetEntityByRenderId(rid);
        return (T)entity;
    }
    
    public SceneEntity GetEntityByRenderId(uint rid)
    {
        var ent = Entities.GetValueOrDefault(rid);
        
        if (ent == null)
        {
            throw new InvalidDataException($"Entity with render id {rid} not found");
        }

        return ent;
    }

    public T GetDraggableEntity<T>(uint rid) where T: DraggableSceneEntity
    {
        var obj = GetEntityOrConnectionSeg<T>(rid);
        if (obj == null) throw new InvalidDataException($"Object with render id {rid} not found");
        return obj;
    }
    
    public T GetEntityOrConnectionSeg<T>(uint rid) where T: SceneEntity
    {
        var entity = GetEntityOrNull(rid);
        if (entity != null) return (T)entity;
        
        entity = ConnectionSegments.GetValueOrDefault(rid);
        
        if (entity == null)
        {
            throw new InvalidDataException($"Entity with render id {rid} not found");
        }
        
        return (T)entity;
    }
    
    public SlotEntity GetSlotEntityByRenderId(uint rid)
    {
        var ent = SlotEntities.GetValueOrDefault(rid);

        if (ent == null)
        {
            throw new InvalidDataException($"Entity with render id {rid} not found");
        }

        return ent;
    }
    
    public void StartDrag(uint entityId, Vector2 worldMousePos)
    {
        DragData.IsDragging = true;
        DragData.EntityId = entityId;
        DragData.StartPosition = MousePosition;
        
        var ent = GetDraggableEntity<DraggableSceneEntity>(entityId);
        DragData.DragOffset = ent.GetOffset(worldMousePos);
    }
    
    public void EndDrag()
    {
        DragData.IsDragging = false;
    }
    
    public void StartConnection(uint startEntityId)
    {
        ConnectionData.IsConnecting = true;
        ConnectionData.StartEntityId = startEntityId;
        ConnectionData.StartPoint = ((SlotSketch)GetSlotEntityByRenderId(startEntityId)).AbsPosition;
    }

    public void EndConnection(bool join = false)
    {
        ConnectionData.IsConnecting = false;
        
        if(!join) return;

        var startSlot = Instance.GetSlotEntityByRenderId(ConnectionData.StartEntityId);
        var endSlot = Instance.GetSlotEntityByRenderId(ConnectionData.EndEntityId);
        
        if(startSlot.IsInput && endSlot.IsInput || !startSlot.IsInput && !endSlot.IsInput)
        {
            return;
        }
        
        var newConnectionEntry = new NewConnectionEntry(startSlot.ParentId, startSlot.Index, endSlot.ParentId, endSlot.Index, startSlot.IsInput);
        Instance.NewConnectionEntries.Add(newConnectionEntry);
    }
    
    public void SetMousePosition(float x, float y)
    {
        MousePosition = new Vector2(x, y);
    }
    
    public void FillNewConnectionEntry(out List<NewConnectionEntry> entries)
    {
        entries = new List<NewConnectionEntry>(NewConnectionEntries);
        NewConnectionEntries.Clear();
    }

    private void SetSelectedEntity(uint rid)
    {
        _selectedEntityId = rid;
        
        foreach (var action in OnSelectedEntityChangedCB)
        {
            action(rid);
        }
    }
    
    public void SetHoveredEntityId(uint rid)
    {
        if (ConnectionSegments.TryGetValue(rid, out var segment))
        {
            SecondaryHoveredEntityId = rid;
            rid = segment.ParentId;
        }
        else
        {
            SecondaryHoveredEntityId = 0;
        }
        
        HoveredEntityId = rid;
    }
    
}