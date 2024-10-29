using System;
using CommunityToolkit.Mvvm.ComponentModel;

namespace Bess.ViewModels;

public partial class MainWindowViewModel : ViewModelBase
{
    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(ZoomStr))]
    public int _zoom = 10;

    public string ZoomStr => $"{Zoom * 10} %";
}