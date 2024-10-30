using BessScene.SceneCore;

namespace BessScene;

public class SceneState
{
    private List<SceneEntity> _sceneEntities = null!;
    
    public static SceneState Instance { get; } = new();
    
    public SceneState()
    {
        Init();   
    }

    public void Init()
    {
        _sceneEntities = new List<SceneEntity>();
    }
    
    public void AddEntity(SceneEntity entity)
    {
        _sceneEntities.Add(entity);
    }
    
    public void RemoveEntity(SceneEntity entity)
    {
        _sceneEntities.Remove(entity);
    }
    
    public void ClearEntities()
    {
        _sceneEntities.Clear();
    }
    
    public List<SceneEntity> Entities => _sceneEntities;
    
    public SceneEntity? GetEntityByRenderId(uint rid)
    {
        return _sceneEntities.FirstOrDefault(entity => entity.RenderId == rid);
    }
}