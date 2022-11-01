part of components;

class NorGate extends Gate {
  NorGate(String id) {
    properties = GateProperties();
    properties.name = "Nor Gate";
    properties.id = id;
    properties.type = ComponentType.norGate;
  }

  static void create(BuildContext context) {
    var gateId = Component.uuid.v4();

    Map<String, Pin> pins = {};
    int index = 0;
    // INPUT PINS
    var pinInIds = List.generate(2, (index) => Component.uuid.v4());
    for(var id in pinInIds){
      var pin = Pin(id);
      (pin.properties as PinProperties).parentId = gateId;
      (pin.properties as PinProperties).behaviour = PinBehaviour.input;
      (pin.properties as PinProperties).width = 40.0;
      (pin.properties as PinProperties).offset = (index == 1)
          ? const Offset(0.0, 74.0)
          : const Offset(0.0, 24.0);
      pins[id] = pin;
      index += 1;
    }

    // OUTPUT PINS
    var pinOutIds = [Component.uuid.v4()];
    for(var id in pinOutIds){
      var pin = Pin(id);
      (pin.properties as PinProperties).parentId = gateId;
      (pin.properties as PinProperties).behaviour = PinBehaviour.output;
      (pin.properties as PinProperties).offset = const Offset(150, 49);
      pins[id] = pin;
    }

    var drawAreaData = Provider.of<DrawAreaData>(context, listen: false);
    drawAreaData.addComponents(pins);

    var gate = NorGate(gateId);
    (gate.properties as GateProperties).inputPins = pinInIds;
    (gate.properties as GateProperties).outputPins = pinOutIds;

    drawAreaData.addComponent(gateId, gate);
  }

  @override
  Widget draw(BuildContext context) {
    return NorGateWidget(
      id: properties.id,
      gateObj: this,
    );
  }

  @override
  void simulate(BuildContext context, DigitalState state, String callerId) {
    var drawAreaData = Provider.of<DrawAreaData>(context, listen: false);
    var properties = this.properties as GateProperties;

    bool? v_;
    for (var pinId in properties.inputPins) {
      var pinProperties = (drawAreaData.components[pinId] as Pin).properties as PinProperties;
      var v = pinProperties.state == DigitalState.high;
      if (v_ == null) {
        v_ = v;
      }else{
        v_ = v_ || v;
      }

      if(v_) break;
    }

    v_ = !v_!;
    DigitalState value = v_ ? DigitalState.high : DigitalState.low;

    for(var pinId in properties.outputPins){
      if(callerId == pinId) continue;
      (drawAreaData.components[pinId] as Pin).simulate(context, value, properties.id);
    }
  }

  @override
  void remove(BuildContext context) {
    var drawAreaData = Provider.of<DrawAreaData>(context);
    var props = properties as GateProperties;
    for(var pinId in props.inputPins){
      drawAreaData.components[pinId]!.remove(context);
    }
    for(var pinId in props.outputPins) {
      drawAreaData.components[pinId]!.remove(context);
    }
    drawAreaData.removeComponent(properties.id);
  }
}
