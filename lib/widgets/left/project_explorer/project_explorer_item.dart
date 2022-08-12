import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/data/draw_area/objects/draw_area_object.dart';
import 'package:bess/themes.dart';
import 'package:bess/widgets/mwidgets/m_iconbutton.dart';
import 'package:flutter/material.dart';

class ProjectExplorerItem extends StatefulWidget {
  const ProjectExplorerItem({
    Key? key,
    required this.drawAreaData,
    required this.text,
    required this.assocId,
  }) : super(key: key);

  final DrawAreaData drawAreaData;
  final String text;
  final String assocId;

  @override
  State<ProjectExplorerItem> createState() => _ProjectExplorerItemState();
}

class _ProjectExplorerItemState extends State<ProjectExplorerItem> {
  bool hovered = false;
  bool expanded = false;
  late DrawAreaObject item;
  @override
  void initState() {
    item = widget.drawAreaData.objects[widget.assocId]!;
    super.initState();
  }

  @override
  Widget build(BuildContext context) {
    return MouseRegion(
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
      child: GestureDetector(
        onTap: () {
          widget.drawAreaData.setSelectedItemId(widget.assocId);
        },
        onDoubleTap: () {
          setState(() {
            expanded = !expanded;
          });
        },
        child: AnimatedContainer(
          duration: const Duration(milliseconds: 100),
          padding: const EdgeInsets.symmetric(horizontal: 10.0, vertical: 5.0),
          decoration: BoxDecoration(
            borderRadius: BorderRadius.all(
              Radius.circular(
                MyTheme.primaryBorderRadius,
              ),
            ),
            color: widget.assocId == widget.drawAreaData.selectedItemId
                ? MyTheme.highlightColor
                : hovered
                    ? MyTheme.secondaryBgColor
                    : null,
          ),
          child: Column(
            children: [
              Tooltip(
                message: item.id,
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    Text(
                      widget.text,
                      style: Theme.of(context).textTheme.titleSmall!.copyWith(
                            color: hovered &&
                                    widget.assocId !=
                                        widget.drawAreaData.selectedItemId
                                ? MyTheme.invertedTextColor
                                : MyTheme.primaryTextColor,
                          ),
                    ),
                    Row(
                      children: [
                        SizedBox(
                          width: 24,
                          height: 24,
                          child: hovered
                              ? MIconButton(
                                  onClicked: () {
                                    widget.drawAreaData
                                        .removeItem(widget.assocId);
                                  },
                                  color: Colors.red,
                                  icon: Icons.close_rounded)
                              : null,
                        ),
                        SizedBox(
                          width: 18.0,
                          height: 18.0,
                          child: IconButton(
                            padding: EdgeInsets.zero,
                            onPressed: () {
                              setState(() {
                                expanded = !expanded;
                              });
                              if (widget.assocId !=
                                  widget.drawAreaData.selectedItemId) {
                                widget.drawAreaData
                                    .setSelectedItemId(widget.assocId);
                              }
                            },
                            color: hovered &&
                                    widget.assocId !=
                                        widget.drawAreaData.selectedItemId
                                ? MyTheme.invertedTextColor
                                : MyTheme.secondaryTextColor,
                            icon: Icon(
                              expanded &&
                                      widget.assocId ==
                                          widget.drawAreaData.selectedItemId
                                  ? Icons.arrow_drop_up_rounded
                                  : Icons.arrow_drop_down_rounded,
                              size: 18.0,
                            ),
                            tooltip: expanded ? "Collapse" : "Expand",
                          ),
                        ),
                      ],
                    )
                  ],
                ),
              ),
              expanded && widget.assocId == widget.drawAreaData.selectedItemId
                  ? PropertiesPanel(item: item)
                  : const SizedBox.shrink(),
            ],
          ),
        ),
      ),
    );
  }
}

class PropertiesPanel extends StatelessWidget {
  const PropertiesPanel({
    Key? key,
    required this.item,
  }) : super(key: key);

  final DrawAreaObject item;

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        const SizedBox(
          height: 5.0,
        ),
        Text(
          "Id: ${item.id}",
          style: Theme.of(context).textTheme.titleSmall!.copyWith(
                color: MyTheme.secondaryTextColor,
              ),
          overflow: TextOverflow.ellipsis,
        )
      ],
    );
  }
}
