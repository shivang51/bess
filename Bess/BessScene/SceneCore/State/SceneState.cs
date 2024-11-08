using System.Numerics;
using BessScene.SceneCore.Entities;
using BessScene.SceneCore.ShadersCollection;
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
    
    public uint HoveredEntityId { get; set; }
    
    private uint _selectedEntityId = 0;
    
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
        Entities = new List<SceneEntity>();
        SlotEntities = new List<SlotEntity>();
        ConnectionEntities = new List<ConnectionEntity>();
        _selectedEntityId = 0;
    }
    
    public void AddEntity(SceneEntity entity)
    {
        Entities.Add(entity);
    }
    
    public void RemoveEntity(SceneEntity entity)
    {
        Entities.Remove(entity);
    }
    
    public void ClearEntities()
    {
        Entities.Clear();
    }

    public List<ConnectionEntity> ConnectionEntities { get; private set; } = null!;
    
    public List<SceneEntity> Entities { get; private set; } = null!;
    
    /// <summary>
    ///  Entities that are children of other entities in Entities list 
    /// </summary>
    public List<SlotEntity> SlotEntities { get; private set; } = null!;

    public bool IsParentEntity(uint rid)
    {
        return Entities.Any(entity => entity.RenderId == rid);
    }
    
    public bool IsSlotEntity(uint rid)
    {
        return SlotEntities.Any(entity => entity.RenderId == rid);
    }
    
    public void AddSlotEntity(SlotEntity entity)
    {
        SlotEntities.Add(entity);
    }
    
    public void RemoveSlotEntity(SlotEntity entity)
    {
        SlotEntities.Remove(entity);
    }
    
    
    /// <summary>
    /// Searches all the entities -> Entities + ChildEntities and returns null if not found
    /// </summary>
    /// <param name="rid">Render ID of entity to be searched</param>
    /// <returns></returns>
    public SceneEntity? GetEntityOrNull(uint rid)
    {
        var entity = Entities.FirstOrDefault(ent => ent.RenderId == rid);
        entity ??= SlotEntities.FirstOrDefault(ent => ent.RenderId == rid);
        return entity;
    }
    
    public SceneEntity GetEntityByRenderId(uint rid)
    {
        var ent = Entities.FirstOrDefault(entity => entity.RenderId == rid);
        
        if (ent == null)
        {
            throw new InvalidDataException($"Entity with render id {rid} not found");
        }

        return ent;
    }

    public T GetEntityByRenderId<T>(uint rid) where T: SceneEntity
    {
        return (T)GetEntityByRenderId(rid);
    }
    
    public SlotEntity GetSlotEntityByRenderId(uint rid)
    {
        var ent = SlotEntities.FirstOrDefault(entity => entity.RenderId == rid);

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
        DragData.DragOffset = worldMousePos - GetEntityByRenderId(entityId).Position;
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

        new ConnectionSketch(ConnectionData.StartEntityId, ConnectionData.EndEntityId);
    }
    
    public void SetMousePosition(float x, float y)
    {
        MousePosition = new Vector2(x, y);
    }

    private void SetSelectedEntity(uint rid)
    {
        _selectedEntityId = rid;
    }
    
}