import 'package:bess/themes.dart';
import 'package:flutter/material.dart';

class MenuItemWidget extends StatefulWidget {
  const MenuItemWidget({
    Key? key,
    required this.title,
    this.shortcut,
    required this.onTap,
  }) : super(
          key: key,
        );

  final String title;
  final String? shortcut;
  final void Function() onTap;

  @override
  State<MenuItemWidget> createState() => _MenuItemWidgetState();
}

class _MenuItemWidgetState extends State<MenuItemWidget> {
  bool hovered = false;

  @override
  Widget build(BuildContext context) {
    return PopupMenuItem(
      padding: EdgeInsets.zero,
      height: 25.0,
      onTap: widget.onTap,
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
        child: Container(
          padding: const EdgeInsets.symmetric(horizontal: 16.0, vertical: 2.0),
          width: double.infinity,
          // color: hovered ? MyTheme.primarySelectionColor : Colors.transparent,
          child: Row(
            children: [
              const SizedBox(
                width: 10.0,
              ),
              Expanded(
                child: Text(
                  widget.title,
                  style: const TextStyle(fontSize: 14.0),
                ),
              ),
              const SizedBox(
                width: 100.0,
              ),
              Text(
                widget.shortcut ?? "",
                style: const TextStyle(fontSize: 14.0),
              ),
              const SizedBox(
                width: 10.0,
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class MenuItemDivider extends PopupMenuItem<Container> {
  MenuItemDivider({
    Key? key,
  }) : super(
          key: key,
          enabled: false,
          child: PopupMenuItem(
            padding: EdgeInsets.zero,
            height: 6.0,
            child: Container(
              // color: MyTheme.primaryTextColor.withAlpha(100),
              height: 1.0,
            ),
          ),
        );
}
