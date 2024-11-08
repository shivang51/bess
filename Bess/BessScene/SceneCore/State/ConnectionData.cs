using System.Numerics;

namespace BessScene.SceneCore.State;

public class ConnectionData
{   
    public bool IsConnecting { get; set; }
    public uint StartEntityId { get; set; }
    public Vector2 StartPoint { get; set; }
    public uint EndEntityId { get; set; }
}