using System;
using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Data.Converters;
using Avalonia.Media;

namespace Bess.Converters;

public class StringToResourceConverter: IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        if (value is string resourceName && !string.IsNullOrEmpty(resourceName))
        {
            Application.Current!.TryFindResource(value, out var val);
            return val!;
        }
        return null; // Return null if resourceName is empty, so PathIcon won't render.
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture) => throw new NotImplementedException();
}