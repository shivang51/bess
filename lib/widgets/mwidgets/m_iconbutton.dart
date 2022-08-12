import 'package:bess/themes.dart';
import 'package:flutter/material.dart';

class MIconButton extends StatelessWidget {
  const MIconButton({
    Key? key,
    required this.onClicked,
    required this.color,
    required this.icon,
    this.tooltip,
  }) : super(key: key);

  final void Function() onClicked;
  final Color color;
  final IconData icon;
  final String? tooltip;

  @override
  Widget build(BuildContext context) {
    return Material(
      color: Colors.transparent,
      child: IconButton(
        padding: EdgeInsets.zero,
        tooltip: tooltip,
        splashRadius: 15.0,
        splashColor: MyTheme.secondaryBgColor,
        highlightColor: MyTheme.secondaryBgColor.withAlpha(200),
        color: color,
        onPressed: onClicked,
        icon: Icon(icon),
      ),
    );
  }
}
