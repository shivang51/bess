using System;
using System.IO;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Platform.Storage;

namespace Bess.ViewModels.MainWindowViewModels;

public class MenuBarViewModel: ViewModelBase
{
    private static readonly FilePickerFileType BessProjectFileType = new("Bess Project")
    {
        Patterns = new []{ "*.proj" }
    };
    
    public void OnFileOpen(Visual? visual)
    {
        var topLevel = TopLevel.GetTopLevel(visual);
        if (topLevel == null)
        {
            Console.Error.WriteLine("TopLevel is null");
            return;
        }
        
        var files = topLevel.StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = "Open Bess Project File",
            AllowMultiple = false,
            FileTypeFilter = new []{BessProjectFileType, FilePickerFileTypes.All}
        }).Result;

        if (files.Count < 1) return;
     
        var file = files[0];
        Console.WriteLine($"File Opened {file.Name}");
    }
}