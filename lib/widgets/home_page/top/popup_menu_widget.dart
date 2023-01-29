import 'package:flutter/material.dart';

class PopUpMenuWidget extends StatefulWidget {
  const PopUpMenuWidget({
    Key? key,
    required this.name,
    required this.menuItems,
    this.onSelected,
  }) : super(key: key);

  final String name;
  final List<PopupMenuEntry> menuItems;
  final void Function(dynamic)? onSelected;

  @override
  State<PopUpMenuWidget> createState() => _PopUpMenuWidgetState();
}

class _PopUpMenuWidgetState extends State<PopUpMenuWidget> {
  bool hovered = false;

  @override
  Widget build(BuildContext context) {
    return PopupMenuButton(
      onSelected: widget.onSelected,
      onCanceled: () => debugPrint("Cancelled"),
      padding: EdgeInsets.zero,
      offset: const Offset(4.0, 24.0),
      tooltip: widget.name,
      itemBuilder: (BuildContext context) => widget.menuItems,
      child: MouseRegion(
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
        child: ClipRRect(
          borderRadius: BorderRadius.circular(5.0),
          child: Container(
            // color: hovered ? MyTheme.highlightColor : Colors.transparent,
            padding: const EdgeInsets.symmetric(
              horizontal: 10.0,
              vertical: 2.0,
            ),
            child: Text(
              widget.name,
              style: const TextStyle(
                // color: MyTheme.secondaryTextColor,
                fontSize: 14.0,
              ),
            ),
          ),
        ),
      ),
    );
  }
}
