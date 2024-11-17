
using BessSimEngine.Components;
using BessSimEngine.Components.Slots;

namespace BessSimEngine;

public class SimEngineState
{
    static SimEngineState()
    {
        Instance = new SimEngineState();
    }
    
    public static SimEngineState Instance { get; }
    
    public List<Component> Components { get; } = new();
    
    public List<Guid> DigitalInputs { get; } = new();
    
    public List<Slot> Slots { get; } = new();
    
    public PriorityQueue<Component, DateTime> SimulationQueue { get;} = new();

    private List<ChangeEntry> ChangeEntries { get; } = new();

    public void Init()
    {
        Components.Clear();
        DigitalInputs.Clear();
        SimulationQueue.Clear();
        Slots.Clear();
        ChangeEntries.Clear();
    }
    
    public void AddComponent(Component component)
    {
        Components.Add(component);
    }
    
    public void RemoveComponent(Guid id)
    {
        GetComponent<Component>(id).Remove();
    }
    
    public void ScheduleSim(Component component, DateTime time)
    {
        SimulationQueue.Enqueue(component, time);
    }

    public TComponent GetComponent<TComponent>(Guid id) where TComponent : Component
    {
        var component = Components.FirstOrDefault(c => c.Id == id);
        if (component == null)
        {
            throw new ArgumentException("Component not found with id: " + id);
        }
        return (TComponent)component;
    }
    
    public Slot GetSlot(Guid id)
    {
        return GetSlot<Slot>(id);
    }
    
    public TComponent GetSlot<TComponent>(Guid id) where TComponent : Slot
    {
        var component = Slots.FirstOrDefault(c => c.Id == id);
        if (component == null)
        {
            throw new ArgumentException("Slot not found with id: " + id);
        }
        return (TComponent)component;
    }
    
    public bool AddConnection(Guid source, Guid target)
    {
        var sourceComponent = GetSlot(source);
        var targetComponent = GetSlot(target);
        
        if(!IsValidConnection(sourceComponent, targetComponent))
        {
            return false;
        }
        
        sourceComponent.AddConnection(target);
        targetComponent.AddConnection(source);

        if (sourceComponent.IsInput)
        {
            sourceComponent.SetState(targetComponent.State);
        }
        else
        {
            targetComponent.SetState(sourceComponent.State);
        }
        
        return true;
    }
    
    private static bool IsValidConnection(Slot source, Slot target)
    {
        var isNotSameType = (source.IsInput && target.IsOutput) || (source.IsOutput && target.IsInput);
        var isNotSameParent = source.ParentId != target.ParentId;
        var isNotConnected = !source.Connections.Contains(target.Id);
        return isNotSameType && isNotSameParent && isNotConnected;
    }
    
    public void RemoveConnection(Guid source, Guid target)
    {
        var sourceComponent = GetSlot(source);
        var targetComponent = GetSlot(target);
        
        sourceComponent.RemoveConnection(target);
        targetComponent.RemoveConnection(source);
    }
    
    internal void AddChangeEntry(ChangeEntry entry)
    {
        ChangeEntries.Add(entry);
    }
    
    public void FillChangeEntries(out List<ChangeEntry> entries)
    {
        entries = ChangeEntries.ToList();
        ChangeEntries.Clear();
    }
    
    public Guid GetCompSlotIdAtInd(Guid compId, int slotInd, SlotType type)
    {
        var component = GetComponent<Component>(compId);

        return type == SlotType.Input ? component.GetInputId(slotInd) : component.GetOutputId(slotInd);
    }
}