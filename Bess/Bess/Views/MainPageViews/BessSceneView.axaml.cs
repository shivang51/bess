using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using Bess.Models.ComponentExplorer;
using Bess.ViewModels.MainWindowViewModels;
using CommunityToolkit.Mvvm.ComponentModel;

namespace Bess.Views.MainPageViews;

public partial class BessSceneView : UserControl
{
    public BessSceneView()
    {
        InitializeComponent();
    }
}