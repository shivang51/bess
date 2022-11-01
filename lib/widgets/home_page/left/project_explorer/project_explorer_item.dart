import 'package:bess/components/component.dart';
import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/themes.dart';
import 'package:provider/provider.dart';
import 'package:flutter/material.dart';

class ProjectExplorerItem extends StatefulWidget {
  const ProjectExplorerItem({
    Key? key,
    required this.id, required this.obj,
  }) : super(key: key);

  final String id;
  final Component obj;

  @override
  State<ProjectExplorerItem> createState() => _ProjectExplorerItemState();
}

class _ProjectExplorerItemState extends State<ProjectExplorerItem> {
  var hovered = false;

  void setHovered(bool value) {
    setState(() {
      hovered = value;
    });
  }

  void remove(BuildContext context){
    widget.obj.remove(context);
  }

  @override
  Widget build(BuildContext context) {
    var drawAreaData = Provider.of<DrawAreaData>(context);
    var item = drawAreaData.components[widget.id]!;
    var properties = item.properties;
    return GestureDetector(
      onTap: () => drawAreaData.setSelectedItemId(widget.id),
      child: MouseRegion(
        onEnter: (e) => setHovered(true),
        onExit: (e) => setHovered(false),
        cursor: SystemMouseCursors.click,
        child: Container(
            padding: const EdgeInsets.symmetric(vertical: 4.0, horizontal: 8.0),
            decoration: BoxDecoration(
              color: hovered
                  || drawAreaData.selectedItemId == widget.id
                  ? MyTheme.highlightColor : null,
              borderRadius: BorderRadius.circular(32.0),
            ),
            child: Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Text(properties.name),
                IconButton(
                  onPressed: () => remove(context),
                  color: Colors.red,
                  padding: EdgeInsets.zero,
                  iconSize: 15.0,
                  constraints: const BoxConstraints(
                    maxWidth: 15.0,
                    maxHeight: 15.0,
                  ),
                  icon: hovered ? const Icon(
                    Icons.highlight_remove_rounded,
                  ) : const SizedBox(),
                ),
              ],
            )),
      ),
    );
  }
}
