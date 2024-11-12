using BessSimEngine.Components.Slots;

namespace BessSimEngine.Components;

public abstract class Component
{
    public Guid Id { get; private set; }
    
    public string Name { get; private set; }
    
    protected List<InputSlot> Inputs { get;}
    
    protected List<OutputSlot> Outputs { get;}
    
    public DateTime NextSimTime { get; private set; }
    
    protected TimeSpan Delay { get; set; } = TimeSpan.Zero;
    
    public abstract void Simulate();

    public abstract List<List<int>> GetState();
    
    public void ScheduleSim()
    {
        NextSimTime = DateTime.Now.Add(Delay);
        SimEngineState.Instance.ScheduleSim(this, NextSimTime);
    }

    protected Component(Guid id, string name, List<InputSlot> inputs, List<OutputSlot> outputs)
    {
        Id = id;
        Name = name;
        Inputs = inputs;
        Outputs = outputs;
    }
    
    protected Component(string name, int inputCount, int outputCount)
    {
        Id = Guid.NewGuid();
        Name = name;
        
        Inputs = new List<InputSlot>();
        for(var i = 0; i < inputCount; i++)
        {
            Inputs.Add(new InputSlot(Id, i));
        }
        
        Outputs = new List<OutputSlot>();
        for(var i = 0; i < outputCount; i++)
        {
            Outputs.Add(new OutputSlot(Id, i));
        }
    }
}