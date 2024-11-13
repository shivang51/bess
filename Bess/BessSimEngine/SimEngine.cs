using System.Diagnostics;
using System.Timers;
using BessSimEngine.Components.DigitalComponents;
using Timer = System.Timers.Timer;

namespace BessSimEngine;

public class SimEngine
{
    private readonly Timer _timer;
    
    public SimEngine()
    {
        SimEngineState.Instance.Init();
        
        _timer = new Timer(1000.0 / 120.0);
        _timer.Elapsed += ProcessSimQueue;
        _timer.AutoReset = true;
        _timer.Enabled = true;
        _timer.Stop();
    }

    private const string LogPath = @"c:\temp\log_.txt";

    private static void ProcessSimQueue(object? sender, ElapsedEventArgs e)
    {
        var n = SimEngineState.Instance.SimulationQueue.Count;
        if(n == 0) return;
        
        File.AppendAllText(LogPath, $"Processing queue at {DateTime.Now} with {n} elements\n");
        
        var comp = SimEngineState.Instance.SimulationQueue.Dequeue();
        while(comp.NextSimTime <= DateTime.Now)
        {
            File.AppendAllText(LogPath, $"Simulating {comp.Name} at {DateTime.Now}\n");
            comp.Simulate();
            if(SimEngineState.Instance.SimulationQueue.Count == 0) break;
            comp = SimEngineState.Instance.SimulationQueue.Dequeue();
        }
        
        if(comp.NextSimTime > DateTime.Now)
        {
            SimEngineState.Instance.SimulationQueue.Enqueue(comp, comp.NextSimTime);
        }
    }
    
    public static bool Connect(Guid source, Guid target)
    {
        return SimEngineState.Instance.AddConnection(source, target);
    }
    
    public static void RemoveComponent(Guid id)
    {
        SimEngineState.Instance.RemoveComponent(id);
    }
    
    public static bool Connect(Guid start, int slotInd, Guid target, int targetInd, bool isStartSlotInput = false)
    {
        Guid startSlotId;
        Guid endSlotId;
        
        if (isStartSlotInput)
        {
            startSlotId = SimEngineState.Instance.GetCompSlotIdAtInd(start, slotInd, SlotType.Input);
            endSlotId = SimEngineState.Instance.GetCompSlotIdAtInd(target, targetInd, SlotType.Output);
        }
        else
        {
            startSlotId = SimEngineState.Instance.GetCompSlotIdAtInd(start, slotInd, SlotType.Output);
            endSlotId = SimEngineState.Instance.GetCompSlotIdAtInd(target, targetInd, SlotType.Input);
        }
        
        return Connect(startSlotId, endSlotId);
    }
    

    public void Start()
    {
        File.WriteAllText(LogPath, "");
        Resume();
    }

    public static Dictionary<Guid, List<List<int>>> GetState()
    {
        File.AppendAllText(LogPath, "Getting state\n");
        var state = new Dictionary<Guid, List<List<int>>>();
        foreach(var component in SimEngineState.Instance.Components)
        {
            state[component.Id] = component.GetState();
        }
        return state;
    }

    public void Reset()
    {
        SimEngineState.Instance.Init();
        _timer.Stop();
    }

    public void Resume()
    {
        _timer.Start();
    }

    public void Pause()
    {
        _timer.Enabled = false;
    }
}