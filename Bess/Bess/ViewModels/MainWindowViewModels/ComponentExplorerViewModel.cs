using System.Collections.ObjectModel;
using Bess.Models.ComponentExplorer;
using CommunityToolkit.Mvvm.ComponentModel;

namespace Bess.ViewModels.MainWindowViewModels;

public partial class ComponentExplorerViewModel: ViewModelBase
{
    public ObservableCollection<ComponentTreeNode> ComponentTree { get; } =
    [
        new ComponentTreeNode("Digital Gates", [
            new ComponentTreeNode("AND Gate", "AndGate", new AndGateModel()),
            new ComponentTreeNode("OR Gate", "OrGate", new OrGateModel()),
            new ComponentTreeNode("NAND Gate", "NandGate", new NandGateModel()),
            new ComponentTreeNode("NOR Gate", "NorGate", new NorGateModel()),
            new ComponentTreeNode("XOR Gate", "XorGate", new XorGateModel()),
            new ComponentTreeNode("XNOR Gate", "XnorGate", new XnorGateModel()),
            new ComponentTreeNode("NOT Gate", "NotGate", new NotGateModel())
        ])
    ];
    

    [ObservableProperty] 
    public ComponentTreeNode? _selectedComponent = null;
}