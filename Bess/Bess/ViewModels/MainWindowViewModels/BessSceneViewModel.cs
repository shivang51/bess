using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using Bess.Models.ComponentExplorer;
using BessScene.SceneCore;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

namespace Bess.ViewModels.MainWindowViewModels;

public partial class BessSceneViewModel: ViewModelBase
{
    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(ZoomStr))]
    public int _zoom = (int)(CameraController.DefaultZoom * 100);

    [ObservableProperty]
    public float _minZoom = (CameraController.MinZoom * 100);
    
    [ObservableProperty]
    public float _maxZoom = (CameraController.MaxZoom * 100);


    public RelayCommand<float> SetZoomCommand { get; }
    
    
    // public ObservableCollection<AddedComponent> AddedComponents { get; set; }
    
    public BessSceneViewModel()
    {
        SetZoomCommand = new RelayCommand<float>(SetZoom);
        // AddedComponents = new();
    }
    
    public string ZoomStr => $"{Zoom} %";


    private void SetZoom(float val)
    {
        Zoom = (int)val;
    }
}