using BessSimEngine;
using BessSimEngine.Components.DigitalComponents.Gates;

namespace SimRunner;  

class Program
{
    private static readonly SimEngine SimEngine = new();
        
    public static void Main(string[] args)
    {
        
        SimEngine.Start();
        
        var notGate = new NotGate();
        var notGate1 = new NotGate();

        PrintState("Initial");
        
        notGate.SetInputState(0, DigitalState.High);
        
        PrintState("After setting input to High");
        
        notGate.SetInputState(0, DigitalState.Low);
        SimEngine.Connect(notGate.GetOutputId(0), notGate1.GetInputId(0));
        
        PrintState("After setting NotGate input to Low and connecting NotGate to NotGate1");
        
        var andGate = new AndGate();
        
        PrintState("After creating AndGate");
        
        SimEngine.Connect(notGate.GetOutputId(0), andGate.GetInputId(0));
        SimEngine.Connect(notGate.GetOutputId(0), andGate.GetInputId(1));
        
        PrintState("After connecting NotGate to AndGate both inputs");
        
        notGate.SetInputState(0, DigitalState.High);
        
        PrintState("After setting NotGate input to High");
    }
    
    private static void PrintState(string message = "")
    {
        Console.ReadLine();
        var state = SimEngine.GetState();
        Console.Write($"{DateTime.Now} {message} State: ");
        foreach (var component in state)
        {
            Console.WriteLine(component.Key);
            
            var inputs = component.Value[0];
            var outputs = component.Value[1];
            
            Console.WriteLine($"\tInputs\t\tOutputs");
            for(int i = 0; i < Math.Max(inputs.Count, outputs.Count); i++)
            {
                int inp = -1, outp = -1;
                
                if (i < inputs.Count)
                {
                    inp = inputs[i];
                }
                
                if (i < outputs.Count)
                {
                    outp = outputs[i];
                }
                
                var inputStr = inp == -1 ? "" : inp.ToString();
                var outputStr = outp == -1 ? "" : outp.ToString();
                
                Console.WriteLine($"\t{inputStr}\t\t{outputStr}");
            }
        }
    }
}