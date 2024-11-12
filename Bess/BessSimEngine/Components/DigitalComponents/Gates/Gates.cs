namespace BessSimEngine.Components.DigitalComponents.Gates;

public class NotGate: DigitalComponent
{
    public NotGate() : base("Not Gate", 1, 1)
    {
    }

    public override void Simulate()
    {
        Outputs[0].State = Inputs[0].IsHigh ? DigitalState.Low : DigitalState.High;
        Outputs[0].Simulate();
    }
}

public class AndGate : DigitalComponent
{
    public AndGate() : base("AND Gate", 2, 1)
    {
    }

    public override void Simulate()
    {
        var state = Inputs[0].IsHigh && Inputs[1].IsHigh ? DigitalState.High : DigitalState.Low;
        Outputs[0].SetState(state);
    }
}

public class OrGate : DigitalComponent
{
    public OrGate() : base("OR Gate", 2, 1)
    {
    }

    public override void Simulate()
    {
        Outputs[0].State = Inputs[0].IsHigh || Inputs[1].IsHigh ? DigitalState.High : DigitalState.Low;
        Outputs[0].Simulate();
    }
}

public class NandGate : DigitalComponent
{
    public NandGate() : base("NAND Gate", 2, 1)
    {
    }

    public override void Simulate()
    {
        Outputs[0].State = !(Inputs[0].IsHigh && Inputs[1].IsHigh) ? DigitalState.High : DigitalState.Low;
        Outputs[0].Simulate();
    }
}

public class NorGate : DigitalComponent
{
    public NorGate() : base("NOR Gate", 2, 1)
    {
    }

    public override void Simulate()
    {
        Outputs[0].State = !(Inputs[0].IsHigh || Inputs[1].IsHigh) ? DigitalState.High : DigitalState.Low;
        Outputs[0].Simulate();
    }
}

public class XorGate : DigitalComponent
{
    public XorGate() : base("XOR Gate", 2, 1)
    {
    }

    public override void Simulate()
    {
        Outputs[0].State = Inputs[0].IsHigh != Inputs[1].IsHigh ? DigitalState.High : DigitalState.Low;
        Outputs[0].Simulate();
    }
}

public class XnorGate : DigitalComponent
{
    public XnorGate() : base("XNOR Gate", 2, 1)
    {
    }

    public override void Simulate()
    {
        Outputs[0].State = Inputs[0].IsHigh == Inputs[1].IsHigh ? DigitalState.High : DigitalState.Low;
        Outputs[0].Simulate();
    }
}
