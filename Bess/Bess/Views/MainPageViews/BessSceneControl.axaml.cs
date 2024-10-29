using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using CommunityToolkit.Mvvm.ComponentModel;

namespace Bess.Views.MainPageViews;

public partial class BessSceneControl : UserControl
{
    
    public BessSceneControl()
    {
        InitializeComponent();
        var fontFamily = FontFamily;
            Console.WriteLine($"Font Family: {fontFamily}");
    }
}