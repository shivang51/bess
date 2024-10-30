namespace BessSimEngine.Components.DigitalComponents.Gates;

public class NotGate: DigitalComponent
{
    public NotGate() : base("Not Gate", 1, 1)
    {
    }

    public override void Simulate()
    {
        Outputs[0].State = Inputs[0].IsHigh ? DigitalState.Low : DigitalState.High;
    }
}