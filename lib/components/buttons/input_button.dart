part of components;

class InputButton extends Component {
  static int _counter = 0;

  InputButton(String id) {
    properties = ButtonProperties();
    _counter++;
    properties.name = "Input Button $_counter";
    properties.id = id;
    properties.type = ComponentType.inputButton;
  }

  static void create(BuildContext context, Offset pos) {
    String buttonId = Component.uuid.v4();
    var button = InputButton(buttonId);

    // PIN
    String pinId = Component.uuid.v4();
    var pin = Pin(
      pinId,
      behaviour: PinBehaviour.output,
      parentId: buttonId,
    );
    (pin.properties as PinProperties).offset = const Offset(45.0, 10);

    (button.properties as ButtonProperties).pinId = pinId;
    button.properties.pos = pos;

    var drawAreaData = Provider.of<DrawAreaData>(context, listen: false);
    drawAreaData.addComponents({pinId: pin, buttonId: button});
  }

  @override
  Widget draw(BuildContext context) {
    return InputButtonWidget(id: properties.id, buttonObj: this);
  }

  @override
  void simulate(BuildContext context, DigitalState state, String callerId) {
    var drawAreaData = Provider.of<DrawAreaData>(context, listen: false);
    var properties = this.properties as ButtonProperties;
    if (properties.pinId == callerId) return;
    var pin = drawAreaData.components[properties.pinId] as Pin;
    var pinProperties = pin.properties as PinProperties;
    pin.simulate(context, pinProperties.state, properties.id);
  }

  @override
  void remove(BuildContext context) {
    var drawAreaData = Provider.of<DrawAreaData>(context, listen: false);
    var pinId = (properties as ButtonProperties).pinId;
    drawAreaData.components[pinId]!.remove(context);
    drawAreaData.removeComponent(properties.id);
  }
}
