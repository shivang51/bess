
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
    
    public List<Component> Components { get; set;} = new();
    
    public List<Slot> Slots { get; set;} = new();
    
    public PriorityQueue<Component, DateTime> SimulationQueue { get;} = new();

    private List<ChangeEntry> _changeEntries { get; } = new();

    public void Init()
    {
        Components.Clear();
        SimulationQueue.Clear();
        Slots.Clear();
        _changeEntries.Clear();
    }
    
    public void AddComponent(Component component)
    {
        Components.Add(component);
    }
    
    public void RemoveComponent(Guid id)
    {
        Components.RemoveAll(c => c.Id == id);
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
    
    public void AddConnection(Guid source, Guid target)
    {
        var sourceComponent = GetSlot(source);
        var targetComponent = GetSlot(target);
        
        sourceComponent.AddConnection(target);
        targetComponent.AddConnection(source);

        if (sourceComponent.IsInputSlot)
        {
            sourceComponent.SetState(targetComponent.State);
        }
        else
        {
            targetComponent.SetState(sourceComponent.State);
        }
    }
    
    internal void AddChangeEntry(ChangeEntry entry)
    {
        _changeEntries.Add(entry);
    }
    
    public void FillChangeEntries(out List<ChangeEntry> entries)
    {
        entries = _changeEntries.ToList();
        _changeEntries.Clear();
    }
}