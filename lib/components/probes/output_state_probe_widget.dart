part of components;

class OutputStateProbeWidget extends StatefulWidget {
  const OutputStateProbeWidget({Key? key, required this.id, required this.probeObj}) : super(key: key);

  final String id;
  final OutputStateProbe probeObj;
  @override
  State<OutputStateProbeWidget> createState() => _OutputStateProbeWidgetState();
}

class _OutputStateProbeWidgetState extends State<OutputStateProbeWidget> {
  void updatePosition(DrawAreaData data, Offset delta) {
    var newPos = pos + delta;
    if (newPos.dx < 0 || newPos.dy < 0) return;

    setState(() {
      pos = newPos;
      widget.probeObj.properties.pos = pos;
      data.updateComponentProperty(widget.id, ComponentPropertyType.pos, pos);
    });
  }

  bool hovered = false;
  Offset pos = Offset.zero;


  @override
  Widget build(BuildContext context) {
    DrawAreaData drawAreaData = Provider.of<DrawAreaData>(context);
    AppData appData = Provider.of<AppData>(context);

    var simulating = appData.simulationState != SimulationState.stopped;
    var properties = widget.probeObj.properties as ProbeProperties;
    pos = properties.pos;
    var pin = drawAreaData.components[properties.pinId]! as Pin;

    bool high = (pin.properties as PinProperties).state == DigitalState.high;
    return Positioned(
      left: pos.dx,
      top: pos.dy,
      child: GestureDetector(
        onPanUpdate:
        !simulating ? (e) => updatePosition(drawAreaData, e.delta) : null,
        child: Row(
          children: [
            pin.draw(context),
            MouseRegion(
              onEnter: (e) {
                setState(() {
                  hovered = true;
                });
              },
              onExit: (e) {
                setState(() {
                  hovered = false;
                });
              },
              cursor: hovered ? SystemMouseCursors.click : MouseCursor.defer,
              child: Tooltip(
                message: high ? "High" : "Low",
                child: Container(
                  width: 25.0,
                  height: 25.0,
                  decoration: BoxDecoration(
                    border: Border.all(color: MyTheme.borderColor),
                    color: high ? Colors.red : Colors.transparent,
                  ),
                  child: Center(
                    child: Text(
                      high ? "H" : "L",
                      style: Theme.of(context).textTheme.titleSmall,
                      overflow: TextOverflow.ellipsis,
                    ),
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
