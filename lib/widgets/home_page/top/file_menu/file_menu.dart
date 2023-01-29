import 'package:bess/pages/splash_page.dart';
import 'package:flutter/material.dart';

import '../popup_menu_widget.dart';
import '../menu_bar_widgets/menu_bar_widgets.dart';

class FileMenu extends StatelessWidget {
  const FileMenu({Key? key}) : super(key: key);

  void onSelected(BuildContext context, e){
    int v = e as int;

    if(v == 3){
      Navigator.of(context).pushNamed(SplashPage.id);
    }

  }

  @override
  Widget build(BuildContext context) {
    List<PopupMenuEntry> fileMenuItems = [
      const MenuButton(
        value: 0,
        label: "New Project",
        shortcutLabel: "Ctrl+N",
      ),
      const MenuButton(
        value: 1,
        label: "Open Project",
        shortcutLabel: "Ctrl+O",
      ),
      const MenuButton(
        value: 2,
        label: "Save Project",
        shortcutLabel: "Ctrl+S",
      ),
      const PopupMenuDivider(),
      const MenuButton(
        value: 3,
        label: "Splash Window",
      ),
    ];
    return PopUpMenuWidget(
      name: "File",
      menuItems: fileMenuItems,
      onSelected: (e) => onSelected(context, e),
    );
  }
}
