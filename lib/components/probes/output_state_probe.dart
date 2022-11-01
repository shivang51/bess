part of components;

class OutputStateProbe extends Component{
  OutputStateProbe(String id, {String? pinId}){
    properties = ProbeProperties();
    properties.id = id;
    properties.name = "Output Probe";
    properties.type = ComponentType.outputStateProbe;
    (properties as ProbeProperties).pinId = pinId ?? "";
  }

  static void create(BuildContext context){
    String probeId = Component.uuid.v4();

    // PIN - INPUT
    String pinId = Component.uuid.v4();
    var pin = Pin(pinId, parentId: probeId);
    (pin.properties as PinProperties).offset = const Offset(0.0, 11.5);

    var probe = OutputStateProbe(probeId, pinId: pinId);

    var drawAreaData = Provider.of<DrawAreaData>(context, listen: false);
    drawAreaData.addComponents({pinId: pin, probeId: probe});
  }

  @override
  Widget draw(BuildContext context) {
    return OutputStateProbeWidget(id: properties.id, probeObj: this);
  }

  @override
  void remove(BuildContext context) {
    var drawAreaData = Provider.of<DrawAreaData>(context, listen: false);
    var pinId = (properties as ProbeProperties).pinId;
    drawAreaData.components[pinId]!.remove(context);
    drawAreaData.removeComponent(properties.id);
  }

}