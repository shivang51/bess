using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Bess.Views.MainPageViews;
using FluentAvalonia.UI.Windowing;

namespace Bess.Views;

public partial class MainWindow : AppWindow
{
    public MainWindow()
    {
        InitializeComponent();
        TitleBar.ExtendsContentIntoTitleBar = true;
        TitleBar.TitleBarHitTestType = TitleBarHitTestType.Complex;
    }
}
