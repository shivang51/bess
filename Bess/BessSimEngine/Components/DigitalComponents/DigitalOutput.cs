namespace BessSimEngine.Components.DigitalComponents;

public class DigitalOutput: DigitalComponent
{
    public DigitalState State { get; private set; }
    
    public DigitalOutput() : base("Digital Output", 1, 0)
    {
        SimEngineState.Instance.DigitalInputs.Add(Id);
    }

    public override void Simulate()
    {
        State = Inputs[0].State;
    }
}