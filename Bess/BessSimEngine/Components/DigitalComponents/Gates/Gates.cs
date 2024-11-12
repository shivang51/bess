namespace BessSimEngine.Components.DigitalComponents.Gates;

public class NotGate: DigitalComponent
{
    public NotGate() : base("Not Gate", 1, 1)
    {
    }

    public override void Simulate()
    {
        var val = Inputs[0].IsHigh ? DigitalState.Low : DigitalState.High;
        Outputs[0].SetState(val);
    }
}

public class AndGate : DigitalComponent
{
    public AndGate() : base("AND Gate", 2, 1)
    {
    }

    public override void Simulate()
    {
        var val = Inputs[0].IsHigh && Inputs[1].IsHigh ? DigitalState.High : DigitalState.Low;
        Outputs[0].SetState(val);
    }
}

public class OrGate : DigitalComponent
{
    public OrGate() : base("OR Gate", 2, 1)
    {
    }

    public override void Simulate()
    {
        var val = Inputs[0].IsHigh || Inputs[1].IsHigh ? DigitalState.High : DigitalState.Low;
        Outputs[0].SetState(val);
    }
}

public class NandGate : DigitalComponent
{
    public NandGate() : base("NAND Gate", 2, 1)
    {
    }

    public override void Simulate()
    {
        var val = !(Inputs[0].IsHigh && Inputs[1].IsHigh) ? DigitalState.High : DigitalState.Low;
        Outputs[0].SetState(val);
    }
}

public class NorGate : DigitalComponent
{
    public NorGate() : base("NOR Gate", 2, 1)
    {
    }

    public override void Simulate()
    {
        var val = !(Inputs[0].IsHigh || Inputs[1].IsHigh) ? DigitalState.High : DigitalState.Low;
        Outputs[0].SetState(val);
    }
}

public class XorGate : DigitalComponent
{
    public XorGate() : base("XOR Gate", 2, 1)
    {
    }

    public override void Simulate()
    {
        var val = Inputs[0].IsHigh != Inputs[1].IsHigh ? DigitalState.High : DigitalState.Low;
        Outputs[0].SetState(val);
    }
}

public class XnorGate : DigitalComponent
{
    public XnorGate() : base("XNOR Gate", 2, 1)
    {
    }

    public override void Simulate()
    {
        var val = Inputs[0].IsHigh == Inputs[1].IsHigh ? DigitalState.High : DigitalState.Low;
        Outputs[0].SetState(val);
    }
}
