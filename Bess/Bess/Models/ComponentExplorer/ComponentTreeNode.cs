using System.Collections.ObjectModel;

namespace Bess.Models.ComponentExplorer;

public class ComponentTreeNode
{
    public ObservableCollection<ComponentTreeNode>? SubNodes { get; }
    public string Title { get; }
    
    public bool ShowIcon => !string.IsNullOrEmpty(Icon);
    
    public string Icon { get; }
  
    public ComponentTreeNode(string title)
    {
        Title = title;
    }
    
    public ComponentTreeNode(string title, string icon)
    {
        Title = title;
        Icon = icon;
    }

    public ComponentTreeNode(string title, ObservableCollection<ComponentTreeNode> subNodes)
    {
        Title = title;
        SubNodes = subNodes;
    }
}