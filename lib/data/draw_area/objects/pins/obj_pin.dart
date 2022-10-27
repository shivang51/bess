import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/data/draw_area/objects/draw_area_object.dart';
import 'package:bess/data/draw_area/objects/types.dart';
import 'package:flutter/cupertino.dart';
import 'package:provider/provider.dart';

abstract class DrawAreaPin extends DrawAreaObject {
  final String parentId;
  DigitalState? state;

  List<String> connectedPins = [];

  DrawAreaPin({
    required this.parentId,
    required super.name,
    required super.id,
    required super.type,
    this.state = DigitalState.low,
  });

  @override
  void update(BuildContext context, DigitalState state){
    var drawAreaData = Provider.of<DrawAreaData>(context, listen: false);
    drawAreaData.objects[parentId]!.update(context, state);
  }

}
