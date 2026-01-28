from bessplug.bindings._bindings.scene import SceneComponent as NativeSceneComp


class SceneComp(NativeSceneComp):
    """
    Represents a component in a schematic diagram scene.
    """

    def __init__(self):
        super().__init__()
