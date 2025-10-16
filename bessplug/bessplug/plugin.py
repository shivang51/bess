from abc import abstractmethod
from .api.sim_engine.component import Component


class Plugin:
    def __init__(self, name, version):
        self.name = name
        self.version = version

    @abstractmethod
    def on_components_reg_load(self) -> list[Component]:
        '''
        Method is called when components are getting loaded into the simulation engine.
        @return: List[Component] 
        '''
        pass


    def activate(self):
        print(f"Activating plugin: {self.name}")

    def deactivate(self):
        print(f"Deactivating plugin: {self.name}")
