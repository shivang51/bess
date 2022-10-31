part of components;

class NandGateWidget extends StatelessWidget {
  const NandGateWidget({
    Key? key,
    required this.id,
    required this.gateObj,
  }) : super(key: key);

  final String id;
  final NandGate gateObj;

  void updatePosition(DrawAreaData data, Offset pos) {
    if (pos.dx < 0 || pos.dy < 0) return;
    gateObj.properties.pos = pos;
    data.updateComponentProperty(id, ComponentPropertyType.pos, pos);
  }

  @override
  Widget build(BuildContext context) {
    var drawAreaData = Provider.of<DrawAreaData>(context);
    var appData = Provider.of<AppData>(context);

    var pos = gateObj.properties.pos;
    var selected = id == drawAreaData.selectedItemId;
    return Positioned(
      left: pos.dx,
      top: pos.dy,
      child: GestureDetector(
        onPanUpdate: !appData.isInSimulationTab()
            ? (e) => updatePosition(drawAreaData, pos + e.delta)
            : null,
        child: Row(
          children: [
            Pin.drawInput((gateObj.properties as GateProperties).inputPins),
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
                  onTap: () => drawAreaData.setSelectedItemId(id),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        gateObj.properties.name,
                        style: Theme.of(context).textTheme.titleSmall,
                        overflow: TextOverflow.ellipsis,
                      ),
                    ],
                  ),
                ),
              ),
            ),
            Pin.drawOutput((gateObj.properties as GateProperties).outputPins),
          ],
        ),
      ),
    );
  }
}
