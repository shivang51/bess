using System;
using BessScene.SceneCore.State;
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
    
    public BessSceneViewModel()
    {
        SetZoomCommand = new RelayCommand<float>(SetZoom);
    }
    
    public string ZoomStr => $"{Zoom} %";


    private void SetZoom(float val)
    {
        Zoom = (int)val;
    }
}