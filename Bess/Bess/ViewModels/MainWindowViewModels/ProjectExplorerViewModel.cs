using System.Collections.ObjectModel;
using System.Linq;
using Bess.Models.ComponentExplorer;
using BessScene.SceneCore;
using BessSimEngine;
using CommunityToolkit.Mvvm.ComponentModel;

namespace Bess.ViewModels.MainWindowViewModels;

public partial class ProjectExplorerViewModel: ViewModelBase
{
    public ObservableCollection<AddedComponent> AddedComponents { get; } = new();
    
    public ProjectExplorerViewModel()
    {
        SceneState.Instance.OnSelectedEntityChangedCB.Add((rid) =>
        {
            if(!AddedComponents.Any() || rid == SelectedComponent?.RenderId)
            {
                return;
            }

            if (rid == 0)
            {
                SelectedComponent = null;
                return;
            }
            
            SelectedComponent = AddedComponents.FirstOrDefault(c => c.RenderId == rid);
        });
    }
    
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
    
    public void SelectComponent(AddedComponent component)
    {
        SelectedComponent = component;
        SceneState.Instance.SelectedEntityId = component.RenderId;
    }

    [ObservableProperty] public AddedComponent? _selectedComponent = null;
}