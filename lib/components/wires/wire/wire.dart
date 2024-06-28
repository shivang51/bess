part of components;

class Wire extends Component {
  static int _counter = 0;

  Wire(String id, String startPinId, String endPinId) {
    properties = WireProperties();
    _counter++;
    properties.name = "Wire $_counter";
    properties.type = ComponentType.wire;
    properties.id = id;
    (properties as WireProperties).startPinId = startPinId;
    (properties as WireProperties).endPinId = endPinId;
  }

  static void create(BuildContext context, String endPinId) {
    var drawAreaData = Provider.of<ProjectData>(context, listen: false);
    var wireId = Component.uuid.v4();

    String startPinId = drawAreaData.connStartData.startPinId;
    var startPin = drawAreaData.components[startPinId] as Pin;
    var startPinProperties = startPin.properties as PinProperties;

    var endPin = drawAreaData.components[endPinId] as Pin;
    var endPinProperties = endPin.properties as PinProperties;

    if (startPinProperties.behaviour == PinBehaviour.output &&
        endPinProperties.behaviour == PinBehaviour.input) {
      endPinProperties.state = startPinProperties.state;
    } else if (startPinProperties.behaviour == PinBehaviour.input &&
        endPinProperties.behaviour == PinBehaviour.output) {
      startPinProperties.state = endPinProperties.state;
    }

    endPinProperties.connectedWiresIds[wireId] = startPinId;
    startPinProperties.connectedWiresIds[wireId] = endPinId;

    var wire = Wire(wireId, startPinId, endPinId);
    startPinProperties.connectedPinsIds.add(endPinId);
    endPinProperties.connectedPinsIds.add(startPinId);

    drawAreaData.addComponent(wireId, wire);
  }

  @override
  Widget draw(BuildContext context) {
    return WireWidget(id: properties.id, wireObj: this);
  }

  @override
  void remove(BuildContext context) {
    var drawAreaData = Provider.of<ProjectData>(context, listen: false);
    var properties = this.properties as WireProperties;
    drawAreaData.removeComponent(properties.id);
  }
}
