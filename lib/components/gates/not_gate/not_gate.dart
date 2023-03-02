part of components;

class NotGate extends Gate {
  static int _counter = 0;

  NotGate(String id) {
    properties = GateProperties();
    _counter++;
    properties.name = "Not Gate $_counter";
    properties.id = id;
    properties.type = ComponentType.notGate;
  }

  static void create(BuildContext context, Offset pos) {
    var gateId = Component.uuid.v4();

    Map<String, Pin> pins = {};

    // INPUT PINS
    var pinInIds = List.generate(1, (index) => Component.uuid.v4());

    for (var id in pinInIds) {
      var pin = Pin(id);
      (pin.properties as PinProperties).parentId = gateId;
      (pin.properties as PinProperties).behaviour = PinBehaviour.input;
      (pin.properties as PinProperties).offset =
          Offset(0.0, (Defaults.notGateSize.dy / 2) - 1);
      pins[id] = pin;
    }

    // OUTPUT PINS
    var pinOutIds = [Component.uuid.v4()];
    for (var id in pinOutIds) {
      var pin = Pin(id);
      (pin.properties as PinProperties).parentId = gateId;
      (pin.properties as PinProperties).behaviour = PinBehaviour.output;
      (pin.properties as PinProperties).offset =
          Offset(95, (Defaults.notGateSize.dy / 2) - 1);
      (pin.properties as PinProperties).state = Defaults.notGateOutputState;
      pins[id] = pin;
    }

    var drawAreaData = Provider.of<ProjectData>(context, listen: false);
    drawAreaData.addComponents(pins);

    var gate = NotGate(gateId);
    (gate.properties as GateProperties).inputPins = pinInIds;
    (gate.properties as GateProperties).outputPins = pinOutIds;
    (gate.properties as GateProperties).pos = pos;

    drawAreaData.addComponent(gateId, gate);
  }

  @override
  Widget draw(BuildContext context) {
    return super.drawGate(
      context,
      id: properties.id,
      shapeBorder: NotPainter(),
    );
  }

  @override
  void simulate(BuildContext context, DigitalState state, String callerId) {
    var drawAreaData = Provider.of<ProjectData>(context, listen: false);
    var properties = this.properties as GateProperties;

    DigitalState value = DigitalState.low;

    var pinId = properties.inputPins[0];
    var pinProperties =
        (drawAreaData.components[pinId] as Pin).properties as PinProperties;

    if (pinProperties.state == DigitalState.low) value = DigitalState.high;

    for (var pinId in properties.outputPins) {
      if (callerId == pinId) continue;
      (drawAreaData.components[pinId] as Pin)
          .simulate(context, value, properties.id);
    }
  }

  @override
  void remove(BuildContext context) {
    var drawAreaData = Provider.of<ProjectData>(context, listen: false);
    var props = properties as GateProperties;
    for (var pinId in props.inputPins) {
      drawAreaData.components[pinId]!.remove(context);
    }
    for (var pinId in props.outputPins) {
      drawAreaData.components[pinId]!.remove(context);
    }
    drawAreaData.removeComponent(properties.id);
  }
}
