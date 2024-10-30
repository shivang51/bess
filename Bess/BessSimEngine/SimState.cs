
using BessSimEngine.Components;

namespace BessSimEngine;

public class SimState
{
    public static SimState Instance { get; } = new();
    
    private List<Component> Components { get; set;} = new();
    
    public void AddComponent(Component component)
    {
        Components.Add(component);
    }
    
    public void RemoveComponent(Guid id)
    {
        Components.RemoveAll(c => c.Id == id);
    }
    
    public void Simulate()
    {
        foreach (var component in Components)
        {
            component.Simulate();
        }
    }
}