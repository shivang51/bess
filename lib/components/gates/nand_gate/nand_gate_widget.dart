part of components;

class NandGateWidget extends StatefulWidget {
  const NandGateWidget({
    Key? key,
    required this.id,
    required this.gateObj,
  }) : super(key: key);

  final String id;
  final NandGate gateObj;

  @override
  State<NandGateWidget> createState() => _NandGateWidgetState();
}

class _NandGateWidgetState extends State<NandGateWidget> {
  var pos = Offset.zero;
  void updatePosition(DrawAreaData data, Offset delta) {
    var newPos = pos + delta;
    if (newPos.dx < 0 || newPos.dy < 0) return;
    setState(() {
      pos = newPos;
      widget.gateObj.properties.pos = pos;
      data.updateComponentProperty(widget.id, ComponentPropertyType.pos, pos);
    });
  }

  @override
  Widget build(BuildContext context) {
    var drawAreaData = Provider.of<DrawAreaData>(context);
    var appData = Provider.of<AppData>(context);

    pos = widget.gateObj.properties.pos;
    var selected = widget.id == drawAreaData.selectedItemId;
    return Positioned(
      left: pos.dx,
      top: pos.dy,
      child: GestureDetector(
        onPanUpdate: !appData.isInSimulationTab()
            ? (e) => updatePosition(drawAreaData, e.delta)
            : null,
        child: Row(
          children: [
            Pin.drawInput((widget.gateObj.properties as GateProperties).inputPins),
            SizedBox(
              width: 110.0,
              height: 100.0,
              child: Material(
                type: MaterialType.button,
                animationDuration: Duration.zero,
                shape: NandPainter(),
                color: selected
                    ? MyTheme.highlightColor.withAlpha(200)
                    : Colors.transparent,
                child: InkWell(
                  customBorder: NandPainter(),
                  splashColor: Colors.orange,
                  hoverColor: Colors.transparent,
                  onTap: () => drawAreaData.setSelectedItemId(widget.id),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        widget.gateObj.properties.name,
                        style: Theme.of(context).textTheme.titleSmall,
                        overflow: TextOverflow.ellipsis,
                      ),
                    ],
                  ),
                ),
              ),
            ),
            Pin.drawOutput((widget.gateObj.properties as GateProperties).outputPins),
          ],
        ),
      ),
    );
  }
}
