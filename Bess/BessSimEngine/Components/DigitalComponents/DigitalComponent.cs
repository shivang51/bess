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
}