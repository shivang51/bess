part of menu_bar_widgets;

class MenuButton<T> extends PopupMenuItem<T> {
  const MenuButton({
    Key? key,
    required this.label,
    super.value,
    this.shortcutLabel,
  }) : super(key: key, child: null);

  final String label;
  final String? shortcutLabel;

  @override
  PopupMenuItemState<T, MenuButton<T>> createState() => _MenuButtonState<T>();
}

class _MenuButtonState<T> extends PopupMenuItemState<T, MenuButton<T>> {
  @override
  Widget build(BuildContext context) {
    return PopupMenuItem(
      padding: const EdgeInsets.symmetric(horizontal: 8.0),
      height: 25.0,
      value: widget.value,
      child: Column(
        children: [
          const SizedBox(width: 250,),
          Row(
            children: [
              Expanded(
                flex: 2,
                child: Text(
                  widget.label,
                  style: Theme.of(context).textTheme.titleSmall,
                ),
              ),
              Expanded(
                child: Text(
                  widget.shortcutLabel ?? "",
                  style: Theme.of(context).textTheme.titleSmall,
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}
