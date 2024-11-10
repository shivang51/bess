using System;
using System.Collections.ObjectModel;
using BessScene.SceneCore.State;
using BessScene.SceneCore.State.ShadersCollection;
using BessSimEngine;
using BessSimEngine.Components;

namespace Bess.Models.ComponentExplorer;


public class ComponentTreeNode
{
    public ObservableCollection<ComponentTreeNode>? SubNodes { get; }
    
    public string Title { get; }
    
    public bool ShowIcon => !string.IsNullOrEmpty(Icon);
    
    public string Icon { get; }
  
    public ComponentTreeNode(string title, ComponentModel component)
    {
        Title = title;
        GeneratorModel = component;
        Icon = "";
    }
    
    public ComponentTreeNode(string title, string icon, ComponentModel component)
    {
        Title = title;
        Icon = icon;
        GeneratorModel = component;
    }

    public ComponentTreeNode(string title, ObservableCollection<ComponentTreeNode> subNodes)
    {
        Title = title;
        SubNodes = subNodes;
        Icon = "";
    }

    public ComponentModel? GeneratorModel { get; } = null;

    public void OnPress()
    {
        Console.WriteLine("Pressed");
    }
}