namespace BessSimEngine.Components;

public class EmptyComponent: Component
{
    public EmptyComponent() : base("Empty Component", 0, 0)
    {
    }
    
    public override void Simulate()
    {
    }
}