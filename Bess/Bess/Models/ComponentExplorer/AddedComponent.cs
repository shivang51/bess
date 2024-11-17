using System;
using System.Collections.Generic;
using BessScene.SceneCore.State;

namespace Bess.Models.ComponentExplorer;

public class AddedComponent
{
    public uint RenderId { get; set; }
    public Guid ComponentId { get; set; }
    public string Name { get; set; }

    public static Dictionary<Guid, uint> ComponentIdToRenderId { get; } = new();
    
    public static Dictionary<uint, Guid> RenderIdToComponentId { get; } = new();

    public AddedComponent(uint renderId, Guid componentId, string name)
    {
        RenderId = renderId;
        ComponentId = componentId;
        Name = name;
        
        ComponentIdToRenderId[componentId] = renderId;
        RenderIdToComponentId[renderId] = componentId;
    }
    
    
    public void Remove()
    {
        ComponentIdToRenderId.Remove(ComponentId);
        RenderIdToComponentId.Remove(RenderId);
        
        SceneState.Instance.RemoveEntityById(RenderId);
        BessSimEngine.SimEngineState.Instance.RemoveComponent(ComponentId);
    }
}