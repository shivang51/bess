import 'package:bess/data/app/app_data.dart';
import 'package:bess/data/draw_area/objects/io/obj_input_button.dart';
import 'package:bess/data/draw_area/objects/pins/obj_pin.dart';
import 'package:bess/procedures/simulation_procedures.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/data/draw_area/objects/types.dart';
import 'package:bess/themes.dart';

import './draw_pin.dart';

class DrawInputButton extends StatefulWidget {
  const DrawInputButton({
    Key? key,
    required this.id,
    required this.pinId,
    this.simulation = false,
  }) : super(key: key);

  final String id;
  final String pinId;
  final bool simulation;

  @override
  State<DrawInputButton> createState() => _DrawInputButtonState();
}

class _DrawInputButtonState extends State<DrawInputButton> {
  late Offset pos;
  late bool high;
  bool hovered = false;

  @override
  void initState() {
    pos = Offset.zero;
    high = false;
    super.initState();
  }

  @override
  Widget build(BuildContext context) {
    DrawAreaData drawAreaData = Provider.of<DrawAreaData>(context);
    AppData appData = Provider.of<AppData>(context);
    var obj = drawAreaData.objects[widget.id]! as DAOInputButton;
    pos = obj.pos ?? Offset.zero;
    high = (drawAreaData.objects[widget.pinId] as DrawAreaPin).state == DigitalState.high;
    return Positioned(
      left: pos.dx,
      top: pos.dy,
      child: GestureDetector(
        onPanUpdate: widget.simulation
            ? null
            : (e) {
                setState(() {
                  pos += e.delta;
                  drawAreaData.setProperty(
                    widget.id,
                    DrawObjectType.inputButton,
                    DrawElementProperty.pos,
                    pos,
                  );
                });
              },
        onTap: () {
          setState(() {
            high = !high;
            var value = high ? DigitalState.high : DigitalState.low;
            drawAreaData.setProperty(
              widget.pinId,
              DrawObjectType.pinOut,
              DrawElementProperty.state,
              value,
            );
            if(appData.simulationState == SimulationState.running) {
              SimProcedures.refreshSimulation(context, widget.pinId);
            }
          });
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
            DrawPin(
              parentId: widget.id,
              pinPos: pos + const Offset(45.0, 11.5),
              id: widget.pinId,
              type: DrawObjectType.pinOut,
            ),
          ],
        ),
      ),
    );
  }
}
