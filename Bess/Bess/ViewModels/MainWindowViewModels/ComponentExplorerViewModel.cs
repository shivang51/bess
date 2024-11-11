using System.Collections.ObjectModel;
using Bess.Models.ComponentExplorer;
using CommunityToolkit.Mvvm.ComponentModel;

namespace Bess.ViewModels.MainWindowViewModels;

public partial class ComponentExplorerViewModel: ViewModelBase
{
    public ObservableCollection<ComponentTreeNode> ComponentTree { get; } =
    [
        new ("I/O", [
            new ComponentTreeNode("Input Probe"),
            new ComponentTreeNode("Output Probe", "OutputProbe"),
            new ComponentTreeNode("Clock")
        ]),
        new ("Digital Gates", [
            new ComponentTreeNode("AND Gate", "IconAndGate", new AndGateModel()),
            new ComponentTreeNode("OR Gate", "IconOrGate", new OrGateModel()),
            new ComponentTreeNode("NAND Gate", "IconNandGate", new NandGateModel()),
            new ComponentTreeNode("NOR Gate", "IconNorGate", new NorGateModel()),
            new ComponentTreeNode("XOR Gate", "IconXorGate", new XorGateModel()),
            new ComponentTreeNode("XNOR Gate", "IconXnorGate", new XnorGateModel()),
            new ComponentTreeNode("NOT Gate", "IconNotGate", new NotGateModel())
        ]),
        new ("Combinational Circuits", [
            new ComponentTreeNode("Full Adder"),
            new ComponentTreeNode("Half Adder")
        ]),
        new ("Sequential Circuits", [
            new ComponentTreeNode("JK Flip Flop"),
            new ComponentTreeNode("SR Flip Flop")
        ])
    ];

    [ObservableProperty] 
    public ComponentTreeNode? _selectedComponent = null;
}