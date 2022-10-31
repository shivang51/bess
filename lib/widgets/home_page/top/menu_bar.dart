import 'package:bess/pages/splash_page.dart';
import 'package:bess/themes.dart';
import 'package:flutter/material.dart';

class MenuBar extends StatefulWidget {
  const MenuBar({
    Key? key,
  }) : super(key: key);

  @override
  State<MenuBar> createState() => _MenuBarState();
}

class _MenuBarState extends State<MenuBar> {
  List<PopupMenuEntry> _fileMenuItems = [];

  void addPostBuildCallback(BuildContext context) {
    _fileMenuItems = [
      MenuItem(
        title: "New Project",
        shortcut: "Ctrl+N",
        onTap_: () {},
      ),
      MenuItem(
        title: "Open Project",
        shortcut: "Ctrl+O",
        onTap_: () {},
      ),
      MenuItem(
        title: "Save Project",
        shortcut: "Ctrl+S",
        onTap_: () {},
      ),
      MenuItemDivider(),
      MenuItem(
        title: "Start Window",
        onTap_: () {
          Navigator.pushNamed(context, SplashPage.id);
        },
      ),
      MenuItem(
        title: "Preferences",
        onTap_: () {},
      ),
      MenuItemDivider(),
      MenuItem(
        title: "Exit",
        onTap_: () {},
      ),
    ];
  }

  @override
  Widget build(BuildContext context) {
    addPostBuildCallback(context);
    return Container(
      decoration: BoxDecoration(
        color: MyTheme.primaryBgColor,
      ),
      padding: const EdgeInsets.all(3.0),
      child: Row(
        children: [
          MenuButton(
            name: "File",
            menuItems: _fileMenuItems,
          ),
          const MenuButton(
            name: "Edit",
            menuItems: [],
          ),
          const MenuButton(
            name: "Help",
            menuItems: [],
          ),
        ],
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
              color: MyTheme.primaryTextColor.withAlpha(100),
              height: 1.0,
            ),
          ),
        );
}

class MenuItem<T> extends PopupMenuItem<T> {
  const MenuItem({
    Key? key,
    required this.title,
    this.shortcut,
    required this.onTap_,
  }) : super(key: key, enabled: true, child: null);

  final String title;
  final String? shortcut;
  final void Function() onTap_;

  @override
  PopupMenuItemState<T, MenuItem<T>> createState() => _MenuItemState<T>();
}

class _MenuItemState<T> extends PopupMenuItemState<T, MenuItem<T>> {
  bool hovered = false;

  @override
  Widget build(BuildContext context) {
    return PopupMenuItem(
      padding: EdgeInsets.zero,
      height: 25.0,
      onTap: widget.onTap_,
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
          color: hovered ? MyTheme.primarySelectionColor : Colors.transparent,
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

class MenuButton extends StatefulWidget {
  const MenuButton({
    Key? key,
    required this.name,
    required this.menuItems,
  }) : super(key: key);
  final String name;
  final List<PopupMenuEntry> menuItems;

  @override
  State<MenuButton> createState() => _MenuButtonState();
}

class _MenuButtonState extends State<MenuButton> {
  bool hovered = false;

  @override
  Widget build(BuildContext context) {
    return PopupMenuButton(
      padding: EdgeInsets.zero,
      offset: const Offset(4.0, 24.0),
      tooltip: widget.name,
      elevation: 10.0,
      color: MyTheme.highlightColor,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(10.0),
      ),
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
            color: hovered ? MyTheme.highlightColor : Colors.transparent,
            padding: const EdgeInsets.symmetric(
              horizontal: 10.0,
              vertical: 2.0,
            ),
            child: Text(
              widget.name,
              style: TextStyle(
                color: MyTheme.secondaryTextColor,
                fontSize: 14.0,
              ),
            ),
          ),
        ),
      ),
    );
  }
}
