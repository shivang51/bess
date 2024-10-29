using System;
using System.Collections.ObjectModel;
using Bess.Models.ComponentExplorer;
using CommunityToolkit.Mvvm.ComponentModel;
// ReSharper disable MemberCanBePrivate.Global
// ReSharper disable InconsistentNaming

namespace Bess.ViewModels;

public partial class MainWindowViewModel : ViewModelBase
{
    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(ZoomStr))]
    public int _zoom = 10;

    public string ZoomStr => $"{Zoom * 10} %";
    
    public ObservableCollection<ComponentTreeNode> ComponentTree { get; }
    
    public MainWindowViewModel()
    {
        ComponentTree =
        [
            new ComponentTreeNode("Digital Gates", [
                new ComponentTreeNode("AND Gate", "AndGate"),
                new ComponentTreeNode("OR Gate"),
                new ComponentTreeNode("NOT Gate"),
                new ComponentTreeNode("NAND Gate"),
                new ComponentTreeNode("NOR Gate"),
                new ComponentTreeNode("XOR Gate"),
                new ComponentTreeNode("XNOR Gate")
            ])
        ];
    }
}