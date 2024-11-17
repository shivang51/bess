using System;
using System.Collections.ObjectModel;
using System.Collections.Specialized;
using System.Linq;
using Bess.Models.ComponentExplorer;
using Bess.ViewModels.MainWindowViewModels;
using Bess.Views.MainPageViews;
using BessScene.SceneCore.State;
using BessSimEngine;
using CommunityToolkit.Mvvm.ComponentModel;
// ReSharper disable MemberCanBePrivate.Global
// ReSharper disable InconsistentNaming

namespace Bess.ViewModels;

public partial class MainWindowViewModel : ViewModelBase
{
    public MainWindowViewModel()
    {
        _addedComponents.CollectionChanged += AddedComponentsOnCollectionChanged;

        SimEngineState.Instance.OnSimTick += span => ElapsedSimTime = span.ToString(@"hh\:mm\:ss\.fff");
    }
    
    private ObservableCollection<AddedComponent> _addedComponents => ProjectExplorerVm.AddedComponents;

    [ObservableProperty] public ProjectExplorerViewModel _projectExplorerVm = new();
    [ObservableProperty] public ComponentExplorerViewModel _componentExplorerVm = new();
    [ObservableProperty] public BessSceneViewModel _bessSceneVm = new();

    private void AddedComponentsOnCollectionChanged(object? sender, NotifyCollectionChangedEventArgs e)
    {
        ShowHelpText = _addedComponents.Count == 0;
    }

    [ObservableProperty]
    public bool _isNotLinux = !OperatingSystem.IsLinux();
    
    public void AddComponent(ComponentModel? componentModel)
    {
        ProjectExplorerVm.AddComponent(componentModel);
    }

    [ObservableProperty]
    public string _elapsedSimTime = "00:00:00";
    
    [ObservableProperty]
    public bool _showHelpText = true;

    public string HelpText => "Select a component to get started.";
}