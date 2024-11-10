using System;
using System.Collections.ObjectModel;
using System.Linq;
using Bess.Models.ComponentExplorer;
using BessScene.SceneCore.State;
using BessSimEngine;
using CommunityToolkit.Mvvm.ComponentModel;
// ReSharper disable MemberCanBePrivate.Global
// ReSharper disable InconsistentNaming

namespace Bess.ViewModels;

public partial class MainWindowViewModel : ViewModelBase
{
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
        AddedComponents.Remove(component);
        SimEngineState.Instance.RemoveComponent(component.ComponentId);
        SceneState.Instance.RemoveEntityById(component.RenderId);
    }
}