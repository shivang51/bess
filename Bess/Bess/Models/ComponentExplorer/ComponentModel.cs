using System;
using System.Reflection;
using BessScene;
using BessScene.SceneCore;
using BessScene.SceneCore.Sketches;
using BessSimEngine;
using BessSimEngine.Components;
using BessSimEngine.Components.DigitalComponents.Gates;

namespace Bess.Models.ComponentExplorer;

public class ComponentModel
{
    protected string Name { get; init; } = "";
    protected int InputCount { get; init; } = 0;
    protected int OutputCount { get; init; } = 0;

    private static (Guid, uint) Generate(Component simComponent, SceneEntity entity)
    {
        SimState.Instance.AddComponent(simComponent);
        SceneState.Instance.AddEntity(entity);
        
        return (simComponent.Id, entity.RenderId);
    }

    public virtual AddedComponent Generate()
    {
        throw new NotImplementedException();
    }

    protected virtual AddedComponent Generate<TSimComponent>() where TSimComponent : Component, new()
    {
        var component = new TSimComponent();
        var sketch = new GateSketch(Name, InputCount, OutputCount);
        var (id, rId) = Generate(component, sketch);
        return new AddedComponent(rId, id, Name);
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
