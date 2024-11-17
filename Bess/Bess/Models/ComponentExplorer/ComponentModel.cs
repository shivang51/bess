using System;
using System.Reflection;
using BessScene.SceneCore.State;
using BessScene.SceneCore.Entities;
using BessScene.SceneCore.Sketches;
using BessSimEngine;
using BessSimEngine.Components;
using BessSimEngine.Components.DigitalComponents;
using BessSimEngine.Components.DigitalComponents.Gates;

namespace Bess.Models.ComponentExplorer;

public class ComponentModel
{
    protected string Name { get; init; } = "";
    protected int InputCount { get; init; } = 0;
    protected int OutputCount { get; init; } = 0;

    public virtual AddedComponent Generate()
    {
        throw new NotImplementedException();
    }

    protected virtual AddedComponent Generate<TSimComponent>() where TSimComponent : Component, new()
    {
        var component = new TSimComponent();
        var sketch = new GateSketch(Name, InputCount, OutputCount);
        return new AddedComponent(sketch.RenderId, component.Id, Name);
    }
}


public class NotGateModel : ComponentModel
{
    public NotGateModel()
    {
        Name = "NOT Gate";
        InputCount = 1;
        OutputCount = 1;
    }
    
    public override AddedComponent Generate()
    {
        return Generate<NotGate>();
    }
}

public class AndGateModel : ComponentModel
{
    public AndGateModel()
    {
        Name = "AND Gate";
        InputCount = 2;
        OutputCount = 1;
    }

    public override AddedComponent Generate()
    {
        return Generate<AndGate>();
    }
}

public class OrGateModel : ComponentModel
{
    public OrGateModel()
    {
        Name = "OR Gate";
        InputCount = 2;
        OutputCount = 1;
    }

    public override AddedComponent Generate()
    {
        return Generate<OrGate>();
    }
}

public class NandGateModel : ComponentModel
{
    public NandGateModel()
    {
        Name = "NAND Gate";
        InputCount = 2;
        OutputCount = 1;
    }

    public override AddedComponent Generate()
    {
        return Generate<NandGate>();
    }
}

public class NorGateModel : ComponentModel
{
    public NorGateModel()
    {
        Name = "NOR Gate";
        InputCount = 2;
        OutputCount = 1;
    }

    public override AddedComponent Generate()
    {
        return Generate<NorGate>();
    }
}

public class XorGateModel : ComponentModel
{
    public XorGateModel()
    {
        Name = "XOR Gate";
        InputCount = 2;
        OutputCount = 1;
    }

    public override AddedComponent Generate()
    {
        return Generate<XorGate>();
    }
}

public class XnorGateModel : ComponentModel
{
    public XnorGateModel()
    {
        Name = "XNOR Gate";
        InputCount = 2;
        OutputCount = 1;
    }

    public override AddedComponent Generate()
    {
        return Generate<XnorGate>();
    }
}

public class DigitalInputModel : ComponentModel
{
    public DigitalInputModel()
    {
        Name = "Digital Input";
        InputCount = 0;
        OutputCount = 1;
    }
    
    public override AddedComponent Generate()
    {
        return Generate<DigitalInput>();
    }
    
    
    protected override AddedComponent Generate<TSimComponent>()
    {
        var component = new TSimComponent();
        var sketch = new DigitalInputSketch();
        return new AddedComponent(sketch.RenderId, component.Id, Name);
    }
}
