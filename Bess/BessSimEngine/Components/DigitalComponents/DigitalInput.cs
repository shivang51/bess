namespace BessSimEngine.Components.DigitalComponents;

public class DigitalInput : DigitalComponent
{
    public DigitalState State { get; private set; }
    
    public DigitalInput() : base("Digital Input", 0, 1)
    {
        SimEngineState.Instance.DigitalInputs.Add(Id);
    }

    public override void Simulate()
    {
        Outputs[0].SetState(State);
    }
    
    public void SetState(DigitalState state)
    {
        State = state;
        ScheduleSim();
    }
}