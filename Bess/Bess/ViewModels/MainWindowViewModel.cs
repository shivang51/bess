using System;
using System.Collections.ObjectModel;
using System.Linq;
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
    
    
    public ObservableCollection<AddedComponent> AddedComponents { get; } = [];

    public void AddComponent(ComponentModel? componentModel)
    {
        if (componentModel == null) return;
        
        var addedComponent = componentModel.Generate();
        AddedComponents.Add(addedComponent);
    }

}