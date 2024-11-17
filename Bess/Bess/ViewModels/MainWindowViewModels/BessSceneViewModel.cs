using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Numerics;
using Bess.Models.ComponentExplorer;
using BessScene.SceneCore.State;
using BessSimEngine;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

namespace Bess.ViewModels.MainWindowViewModels;

public partial class BessSceneViewModel: ViewModelBase
{
    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(ZoomStr))]
    public int _zoom = (int)(CameraController.DefaultZoom * 100);
    
    public string ZoomStr => $"{Zoom} %";

    [ObservableProperty]
    public float _minZoom = (CameraController.MinZoom * 100);
    
    [ObservableProperty]
    public float _maxZoom = (CameraController.MaxZoom * 100);
    
    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(CameraPosStr))]
    public Vector2 _cameraPos = new(0, 0);
    
    public string CameraPosStr => $"X: {Math.Round(CameraPos.X)}, Y: {Math.Round(CameraPos.Y)}";

    public RelayCommand<float> SetZoomCommand { get; }
    
    public RelayCommand<Vector2> SetCamPosChanged { get; }
    
    public BessSceneViewModel()
    {
        SetZoomCommand = new RelayCommand<float>(SetZoom);
        SetCamPosChanged = new RelayCommand<Vector2>(SetCameraPos);
        // AddedComponents = new();
    }


    private void SetZoom(float val)
    {
        Zoom = (int)val;
    }
    
    private void SetCameraPos(Vector2 pos)
    {
        CameraPos = pos;
    }
    
    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(IsSimRunning))]
    public bool _isSimPaused = false;

    [ObservableProperty]
    public string _buttonIcon = "PauseFilled";
    
    public bool IsSimRunning => !IsSimPaused;

    public void PlayPauseSim()
    {
        if(IsSimPaused) ResumeSim();
        else PauseSim();
    }
    
    public void PauseSim()
    {
        SimEngineState.Instance.PauseSim();
        IsSimPaused = true;
        ButtonIcon = "PlayFilled";
    }
    
    public void ResumeSim()
    {
        SimEngineState.Instance.ResumeSim();
        IsSimPaused = false;
        ButtonIcon = "PauseFilled";
    }
}