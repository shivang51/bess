part of components;

class InputButtonWidget extends StatefulWidget {
  const InputButtonWidget({Key? key, required this.id, required this.buttonObj})
      : super(key: key);

  final String id;
  final InputButton buttonObj;

  @override
  State<InputButtonWidget> createState() => _InputButtonWidgetState();
}

class _InputButtonWidgetState extends State<InputButtonWidget> {
  void updatePosition(DrawAreaData data, Offset delta) {
    var newPos = pos + delta;
    if (newPos.dx < 0 || newPos.dy < 0) return;

    setState(() {
      pos = newPos;
      widget.buttonObj.properties.pos = pos;
      data.updateComponentProperty(widget.id, ComponentPropertyType.pos, pos);
    });
  }

  bool hovered = false;
  Offset pos = Offset.zero;

  @override
  Widget build(BuildContext context) {
    DrawAreaData drawAreaData = Provider.of<DrawAreaData>(context);
    AppData appData = Provider.of<AppData>(context);
    var properties = widget.buttonObj.properties as ButtonProperties;
    pos = properties.pos;
    var pin = drawAreaData.components[properties.pinId]! as Pin;
    bool high = (pin.properties as PinProperties).state == DigitalState.high;
    bool selected = drawAreaData.selectedItemId == widget.id;

    return Positioned(
      left: pos.dx,
      top: pos.dy,
      child: Stack(
        children: [
          selected
              ? Container(
            width: 45,
            height: 25,
            padding: const EdgeInsets.all(8.0),
            decoration: BoxDecoration(
              border: Border.all(color: MyTheme.borderColor),
              color: MyTheme.highlightColor,
            ),
          )
              : const SizedBox(),
          GestureDetector(
            onPanUpdate: !appData.isInSimulationTab()
                ? (e) => updatePosition(drawAreaData, e.delta)
                : null,
            onTap: () {
              high = !high;
              var value = high ? DigitalState.high : DigitalState.low;
              drawAreaData.updateComponentProperty(
                properties.pinId,
                ComponentPropertyType.digitalState,
                value,
              );
              if (appData.simulationState == SimulationState.running) {
                widget.buttonObj.simulate(context, value, widget.id);
              }
            },
            child: Row(
              children: [
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
                        shape: BoxShape.circle,
                        border: Border.all(color: MyTheme.borderColor),
                        color: high ? Colors.red : MyTheme.backgroundColor,
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
                pin.draw(context),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
