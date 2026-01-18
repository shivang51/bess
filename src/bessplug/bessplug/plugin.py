from abc import abstractmethod

from .api.log import Logger
from .api.sim_engine import ComponentDefinition
from .api.scene.schematic_diagram import SchematicDiagram
from .api.assets import AssetManager


class Plugin:
    def __init__(self, name, version):
        self.name = name
        self.version = version
        self.logger = Logger(name)

    def cleanup(self):
        print(f"Cleaning plugin: {self.name} v{self.version}")
        AssetManager.cleanup()

    @abstractmethod
    def on_components_reg_load(self) -> list[ComponentDefinition]:
        """
        Method is called when components are getting loaded into the simulation engine.
        @return: List[Component]
        """
        pass

    @abstractmethod
    def on_scene_comp_load(self) -> dict[int, object]:
        """
        Method is called when scene components are getting loaded for rendered inside scene in schematic view.
        @return: Dict[int, type] mapping of component definition hash to their scene component class
        """
        pass
