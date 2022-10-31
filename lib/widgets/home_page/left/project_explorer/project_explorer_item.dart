import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:provider/provider.dart';
import 'package:flutter/material.dart';

class ProjectExplorerItem extends StatefulWidget {
  const ProjectExplorerItem({
    Key? key, required this.id,
  }) : super(key: key);

  final String id;

  @override
  State<ProjectExplorerItem> createState() => _ProjectExplorerItemState();
}

class _ProjectExplorerItemState extends State<ProjectExplorerItem> {

  @override
  Widget build(BuildContext context) {
    var drawAreaData = Provider.of<DrawAreaData>(context);
    var item = drawAreaData.components[widget.id]!;
    var properties = item.properties;
    return Text(properties.name);
  }
}
