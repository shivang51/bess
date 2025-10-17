from abc import abstractmethod
from enum import Enum

class LogicState(Enum):
    LOW = 0
    HIGH = 1
    UNKNOWN = 2
    HIGH_Z = 3


class ComponentType(Enum):
    UNDEFINED = -1
    DIGITAL_COMPONENT = 0


class PinType(Enum):
    INPUT_PIN = 0
    OUTPUT_PIN = 1


class ExtendedPinType(Enum):
    NONE = -1,
    INPUT_CLOCK = 0,
    INPUT_CLEAR = 1


class ComponentState:
    '''This class defines component state'''
    def __init__(self):
        self.inputs: list[PinState] = []
        '''List of input pin states'''

        self.outputs: list[PinState] = []
        '''List of output pin states'''

        self.lastSimTimeNs: float = 0.0
        '''Last simulation time in nanoseconds'''

        self.compDefination: Component = Component("Undefined")

class PinState:
    '''This class defines pin state'''
    def __init__(self, type: PinType, state: LogicState = LogicState.LOW):
        self.type: PinType = type
        '''Type of the pin (input/output)'''

        self.state: LogicState = state
        '''State of the pin (HIGH/LOW/UNKNOWN/HIGH_Z)'''

        self.lastChangeTimeNs: float = 0.0
        '''Last time (in ns) when the pin state was changed'''


class PinDetails:
    '''Use this class to define pin details for your custom component'''
    def __init__(self, type: PinType, name: str, extended_type: ExtendedPinType = ExtendedPinType.NONE):
        self.name: str = name
        '''Name of the pin'''

        self.type: PinType = type
        '''Type of the pin (input/output)'''

        self.extended_type: ExtendedPinType = extended_type
        '''Extended type of the pin (e.g. clock, clear)'''


class Component:
    '''Use this class to define your custom component'''
    def __init__(self, name):
        self.name: str = name
        '''Name of the component'''

        self.type: ComponentType = ComponentType.UNDEFINED
        '''Type of component, currently only DIGITAL_COMPONENT is supported'''

        self.groupName: str = "Plugin Components"
        ''' Group in which this component should show up in the
         component explorer'''

        self.simDelayNs: float = 0.0
        '''Simulation delay in nano seconds'''

        self.expressions: list[str] = []
        '''List of digital logic expressions.
        Each expression defines one output pin.
        e.g. for a fulladder with 3 input pins expressions will be
        "0^1^2", "(0*1) + 2*(0^1)", where each number represents an input pin index.'''

        self.inputPinsDetails: list[PinDetails] = []
        '''List of input pin details. 
        Extra information about each input pin to show in UI.
        No effect functionally'''

        self.outputPinsDetails: list[PinDetails] = []
        '''List of output pin details.
        Extra information about each output pin to show in UI.
        No effect functionally'''

        self.inputsCount: int = 0
        '''Number of input pins'''

        self.outputsCount: int = 0
        '''Number of output pins'''

        self.operator: str = ''
        '''Operator used in expressions. This should only be used if your component 
        will have only one output pin whose value is determined by this operator 
        applied to all input pins.
        Currently supported operators are (in order precedence, highest first):
        NOT: !
        AND: *
        EXOR: ^
        OR: +
         '''

    @abstractmethod
    def simulate(self, newInputs: list[PinState], simTime: float, 
                 prevState: ComponentState) -> ComponentState:
        '''
        This method is called by the simulation engine to simulate the component logic.
        @param inputPinStates: List[PinState] - List of input pin states
        @param simTime: float - Current simulation time in nanoseconds
        @param prevState: ComponentState - Previous state of the component
        @return: ComponentState - New state of the component (Update both inputs and outputs)
        '''
        pass

    def __repr__(self):
        return f'{{"name": "{self.name}", "type": "{self.type.name}", ' +\
               f'"groupName": "{self.groupName}", "simDelayNs": {self.simDelayNs}, ' +\
               f'"expressions": {self.expressions}, ' +\
               f'"inputsCount": {self.inputsCount}, "outputsCount": {self.outputsCount}, ' +\
                   f'"operator": "{self.operator}"}}'



    def update(self, delta_time):
        print("[BessPlugin][Update]", delta_time);

