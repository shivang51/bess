namespace BessSimEngine.Components;

public class EmptyComponent: Component
{
    public EmptyComponent() : base("Empty Component", 0, 0)
    {
    }
    
    public override void Simulate()
    {
    }
    
    public override List<List<int>> GetState()
    {
        return new List<List<int>>();
    }
    
    public override void Remove()
    {
    }
}