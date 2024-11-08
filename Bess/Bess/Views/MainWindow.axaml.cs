using System;
using System.Collections.Generic;
using System.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Templates;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.VisualTree;
using Bess.Views.MainPageViews;
using FluentAvalonia.UI.Windowing;

namespace Bess.Views;

public partial class MainWindow : AppWindow
{
    public bool IsNotLinux = !OperatingSystem.IsLinux();
    
    public MainWindow()
    {
        InitializeComponent();
        TitleBar.ExtendsContentIntoTitleBar = true;
        TitleBar.TitleBarHitTestType = TitleBarHitTestType.Complex;

        if (OperatingSystem.IsLinux()) return;
        
        Background = Brushes.Transparent;
        TransparencyLevelHint = [WindowTransparencyLevel.AcrylicBlur];

    }

    private IEnumerable<T> GetAllByClass<T>(string className) where T: Control
    {
        return this.GetVisualDescendants().OfType<T>().Where(el => el.Classes.Contains(className));
    }
}
