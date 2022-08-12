import 'draw_objects.dart';

abstract class DrawAreaObject {
  String name = "";
  String id = "";
  DrawObject type = DrawObject.none;
  List<String> connectedItems = [];
  DrawAreaObject({
    required this.name,
    required this.id,
    required this.type,
  });
}
