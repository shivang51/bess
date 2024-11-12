using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;

namespace Bess.Models;

public static class PercentageMargin
{

    public static readonly AttachedProperty<double> PercentageProperty =
        AvaloniaProperty.RegisterAttached<Control, double>(
            "Percentage", typeof(PercentageMargin));

    public static void SetPercentage(Control control, double value) =>
        control.SetValue(PercentageProperty, value);

    public static double GetPercentage(Control control) =>
        control.GetValue(PercentageProperty);

    static PercentageMargin()
    {
        PercentageProperty.Changed.Subscribe(new PercentageChangeObserver());
    }
    
}

public class PercentageChangeObserver : IObserver<AvaloniaPropertyChangedEventArgs<double>>
{
    public void OnCompleted()
    {
        throw new NotImplementedException();
    }

    public void OnError(Exception error)
    {
        throw new NotImplementedException();
    }

    public void OnNext(AvaloniaPropertyChangedEventArgs<double> value)
    {
        if (value.Sender is Control control)
        {
            if (control.Parent is Control parent)
            {
                parent.PropertyChanged += (s, ev) =>
                {
                    if (ev.Property == Layoutable.BoundsProperty)
                    {
                        var parentBounds = parent.Bounds;
                        double percentage = PercentageMargin.GetPercentage(control);
                        control.Margin = new Thickness(
                            parentBounds.Width * percentage,
                            parentBounds.Height * percentage,
                            parentBounds.Width * percentage,
                            parentBounds.Height * percentage);
                    }
                };
            }
        }
    }
}

