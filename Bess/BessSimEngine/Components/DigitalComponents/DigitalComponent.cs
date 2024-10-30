using System.Globalization;

namespace BessSimEngine.Components.DigitalComponents;

public abstract class DigitalComponent: Component
{
    protected DigitalComponent(string name, int inputCount, int outputCount) : base(name, inputCount, outputCount)
    {
    }
}