part of components;

class Pin extends Component{

  Pin(String id, {String? parentId, PinBehaviour? behaviour}){
    properties = PinProperties();
    properties.id = id;
    properties.type = ComponentType.pin;
    (properties as PinProperties).parentId = parentId ?? "";
    (properties as PinProperties).behaviour = behaviour ?? PinBehaviour.input;
  }

  static void create(BuildContext context){
    var id = Component.uuid.v4();
    var drawAreaData = Provider.of<DrawAreaData>(context, listen:false);
    drawAreaData.addComponent(id, Pin(id));
  }

  static Widget drawInput(List<String> ids){
    return PinsInWidget(ids: ids);
  }

  static Widget drawOutput(List<String> ids){
    return PinsOutWidget(ids: ids);
  }

  @override
  void simulate(BuildContext context, DigitalState state, String callerId){
    var drawAreaData = Provider.of<DrawAreaData>(context, listen: false);
    var properties = this.properties as PinProperties;

    drawAreaData.updateComponentProperty(properties.id, ComponentPropertyType.digitalState, state);
    for(var connectedPinId in properties.connectedPinsIds){
      if(connectedPinId == callerId) continue;
      var cProperties = (drawAreaData.components[connectedPinId] as Pin).properties as PinProperties;
      if(cProperties.behaviour == PinBehaviour.input) {
        var connectedPin = drawAreaData.components[connectedPinId] as Pin;
        connectedPin.simulate(context, state, properties.id);
      }
    }

    if(properties.parentId != callerId) {
      drawAreaData.components[properties.parentId]?.simulate(
          context, state, properties.id
      );
    }

  }


  @override
  Widget draw(BuildContext context) {
    return PinWidget(id: properties.id, pinObj: this,);
  }
}