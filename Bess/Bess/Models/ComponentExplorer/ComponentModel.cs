using System;
using BessScene;
using BessScene.SceneCore;
using BessSimEngine;
using BessSimEngine.Components;
using BessSimEngine.Components.DigitalComponents.Gates;

namespace Bess.Models.ComponentExplorer;

public abstract class ComponentModel
{
    protected string Name { get; init; } = "";
    
    protected static (Guid, uint) Generate<TSimComponent, TSceneEntity>() where TSimComponent : Component, new() where TSceneEntity : SceneEntity, new()
    {
        var simComponent = new TSimComponent();
        var entity = new TSceneEntity();
        
        SimState.Instance.AddComponent(simComponent);
        SceneState.Instance.AddEntity(entity);
        
        return (simComponent.Id, entity.RenderId);
    }
    
    public abstract AddedComponent Generate();
}

public class NotGateModel : ComponentModel
{
    public NotGateModel()
    {
        Name = "NOT Gate";
    }
    
    public override AddedComponent Generate()
    {
        Console.WriteLine("Generating NotGate");
        var (id, rId) = Generate<NotGate, DummyEntity>();
        return new AddedComponent(rId, id, Name);
    }
}