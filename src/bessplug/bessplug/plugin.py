from abc import abstractmethod

from .api.log import Logger
from .api.sim_engine import ComponentDefinition
from .api.scene.schematic_diagram import SchematicDiagram


class Plugin:
    def __init__(self, name, version):
        self.name = name
        self.version = version
        self.logger = Logger(name)

    @abstractmethod
    def on_components_reg_load(self) -> list[ComponentDefinition]:
        """
        Method is called when components are getting loaded into the simulation engine.
        @return: List[Component]
        """
        pass

    @abstractmethod
    def on_schematic_symbols_load(self) -> dict[int, SchematicDiagram]:
        """
        Method is called when schematic symbols are getting loaded for rendered inside scene in schematic view.
        @return: Dict[int, Path] mapping of component definition hash to their schematic symbol paths.
        """
        pass

    @abstractmethod
    def on_scene_comp_load(self) -> dict[int, type]:
        """
        Method is called when scene components are getting loaded for rendered inside scene in schematic view.
        @return: Dict[int, type] mapping of component definition hash to their scene component class
        """
        pass
