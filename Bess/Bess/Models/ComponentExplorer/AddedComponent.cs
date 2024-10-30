using System;

namespace Bess.Models.ComponentExplorer;

public class AddedComponent
{
    public uint RenderId { get; set; }
    public Guid ComponentId { get; set; }
    public string Name { get; set; }
    
    public AddedComponent(uint renderId, Guid componentId, string name)
    {
        RenderId = renderId;
        ComponentId = componentId;
        Name = name;
    }
}