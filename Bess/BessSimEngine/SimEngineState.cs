
using BessSimEngine.Components;

namespace BessSimEngine;

public class SimEngineState
{
    static SimEngineState()
    {
        Instance = new SimEngineState();
    }
    
    public static SimEngineState Instance { get; }
    
    public List<Component> Components { get; set;} = new();
    
    private PriorityQueue<Component, DateTime> SimulationQueue { get; set;} = new();

    public void Init()
    {
        Components.Clear();
        SimulationQueue.Clear();
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
}