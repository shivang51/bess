using System.Timers;
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

    private static void ProcessSimQueue(object? sender, ElapsedEventArgs e)
    {
        var components =  SimEngineState.Instance.Components;

        foreach (var component in components.TakeWhile(component => component.NextSimTime <= DateTime.Now))
        {
            component.Simulate();
        }
    }

    public void Start()
    {
        Resume();
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