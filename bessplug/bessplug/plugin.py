from abc import abstractmethod

from .bindings import _bindings as b
from .api.log import Logger
from .api.sim_engine.component import Component


class Plugin:
    def __init__(self, name, version):
        self.name = name
        self.version = version
        self.logger = Logger(name)

    @abstractmethod
    def on_components_reg_load(self) -> list[Component]:
        '''
        Method is called when components are getting loaded into the simulation engine.
        @return: List[Component] 
        '''
        pass


    def activate(self):
        self.logger.log(f"Activating plugin: {self.name} v{self.version}")

    def deactivate(self):
        self.logger.log(f"Deactivating plugin: {self.name} v{self.version}")
