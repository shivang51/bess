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

        if (OperatingSystem.IsLinux())
        {
            Background = new LinearGradientBrush()
            {
                GradientStops =
                [
                    new GradientStop(Color.FromUInt32(0xFF1B1827), 0),
                    new GradientStop(Color.FromUInt32(0xFF11111B), 1),
                ],
                StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
                EndPoint = new RelativePoint(1, 1, RelativeUnit.Relative)
            };
            return;
        }
        
        // Background = Brushes.Transparent;
            Background = new LinearGradientBrush()
            {
                GradientStops =
                [
                    new GradientStop(Color.FromUInt32(0x5F1B1827), 0),
                    new GradientStop(Color.FromUInt32(0x5F11111B), 1),
                ],
                StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
                EndPoint = new RelativePoint(1, 1, RelativeUnit.Relative)
            };
        TransparencyLevelHint = [WindowTransparencyLevel.Mica];

    }

    private IEnumerable<T> GetAllByClass<T>(string className) where T: Control
    {
        return this.GetVisualDescendants().OfType<T>().Where(el => el.Classes.Contains(className));
    }
}
