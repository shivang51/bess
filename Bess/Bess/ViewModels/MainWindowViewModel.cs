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
    public ObservableCollection<AddedComponent> AddedComponents { get; } = [];

    public void AddComponent(ComponentModel? componentModel)
    {
        if (componentModel == null) return;
        
        var addedComponent = componentModel.Generate();
        AddedComponents.Add(addedComponent);
    }

}