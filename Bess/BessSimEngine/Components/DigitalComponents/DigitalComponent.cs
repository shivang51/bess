using System.Globalization;

namespace BessSimEngine.Components.DigitalComponents;

public abstract class DigitalComponent: Component
{
    protected DigitalComponent(string name, int inputCount, int outputCount) : base(name, inputCount, outputCount)
    {
        SimEngineState.Instance.Components.Add(this);
        ScheduleSim();
    }

    public override List<List<int>> GetState()
    {
        var state = new List<List<int>>()
        {
            Inputs.Select(i => i.StateInt).ToList(),
            Outputs.Select(o => o.StateInt).ToList()
        };

        return state;
    }
    
    public void SetInputState(int index, DigitalState state)
    {
        Inputs[index].SetState(state);
    }
    
    public Guid GetOutputId(int index)
    {
        return Outputs[index].Id;
    }
    
    public Guid GetInputId(int index)
    {
        return Inputs[index].Id;
    }

    public override void Remove()
    {
        foreach (var slot in Inputs)
            slot.Remove();
        
        foreach (var slot in Outputs)
            slot.Remove();
        
        SimEngineState.Instance.Components.Remove(this);
    }

    public int GetConnectionsCount()
    {
        var inputConnections = Inputs.Select(i => i.ConnectionCount).Sum();
        var outputConnections = Outputs.Select(o => o.ConnectionCount).Sum();
        
        return inputConnections + outputConnections;
    }
}