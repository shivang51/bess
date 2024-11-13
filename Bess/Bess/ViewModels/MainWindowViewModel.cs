using System;
using System.Collections.ObjectModel;
using System.Collections.Specialized;
using System.Linq;
using Bess.Models.ComponentExplorer;
using BessScene.SceneCore;
using BessSimEngine;
using CommunityToolkit.Mvvm.ComponentModel;
// ReSharper disable MemberCanBePrivate.Global
// ReSharper disable InconsistentNaming

namespace Bess.ViewModels;

public partial class MainWindowViewModel : ViewModelBase
{
    public MainWindowViewModel()
    {
        AddedComponents.CollectionChanged += AddedComponentsOnCollectionChanged;
    }

    private void AddedComponentsOnCollectionChanged(object? sender, NotifyCollectionChangedEventArgs e)
    {
        ShowHelpText = AddedComponents.Count == 0;
    }

    [ObservableProperty]
    public bool _isNotLinux = !OperatingSystem.IsLinux();
    
    public ObservableCollection<AddedComponent> AddedComponents { get; } = [];
    
    public void AddComponent(ComponentModel? componentModel)
    {
        if (componentModel == null) return;
        
        var addedComponent = componentModel.Generate();
        AddedComponents.Add(addedComponent);
    }
    
    public void RemoveComponent(AddedComponent component)
    {
        component.Remove();
        AddedComponents.Remove(component);
    }
    
    [ObservableProperty]
    public bool _showHelpText = true;

    public string HelpText => "Select a component to get started.";
}