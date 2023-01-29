import 'package:flutter/material.dart';

import 'file_menu/file_menu.dart';
import 'popup_menu_widget.dart';

class MenuBar extends StatefulWidget {
  const MenuBar({
    Key? key,
  }) : super(key: key);

  @override
  State<MenuBar> createState() => _MenuBarState();
}

class _MenuBarState extends State<MenuBar> {
  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: const BoxDecoration(
          // color: MyTheme.primaryBgColor,
          ),
      padding: const EdgeInsets.all(3.0),
      child: Row(
        children: const [
          FileMenu(),
          PopUpMenuWidget(
            name: "Edit",
            menuItems: [],
          ),
          PopUpMenuWidget(
            name: "Help",
            menuItems: [],
          ),
        ],
      ),
    );
  }
}
