import 'package:bess/components/component_type.dart';
import 'package:bess/components/components.dart';
import 'package:bess/data/project_data/project_data.dart';
import 'package:bess/data/mouse_data.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

class DrawArea extends StatefulWidget {
  const DrawArea({Key? key}) : super(key: key);

  @override
  State<DrawArea> createState() => _DrawAreaState();
}

class _DrawAreaState extends State<DrawArea> {
  late final ScrollController _hScrollController;
  late final ScrollController _vScrollController;
  late final ScrollController __vScrollController;

  void onScrollUpdate() {
    __vScrollController.jumpTo(_vScrollController.offset);
  }

  void onContainerScroll() {
    _vScrollController.jumpTo(__vScrollController.offset);
  }

  @override
  void initState() {
    _hScrollController = ScrollController(initialScrollOffset: 0);
    _vScrollController = ScrollController(initialScrollOffset: 0);
    __vScrollController = ScrollController(initialScrollOffset: 0);
    _vScrollController.addListener(onScrollUpdate);
    __vScrollController.addListener(onContainerScroll);
    super.initState();
  }

  @override
  void dispose() {
    _vScrollController.removeListener(onScrollUpdate);
    __vScrollController.removeListener(onContainerScroll);
    _hScrollController.dispose();
    _vScrollController.dispose();
    __vScrollController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.all(8.0),
      child: Material(
        elevation: 20.0,
        child: Stack(
          children: [
            Align(
              alignment: Alignment.topLeft,
              child: RawScrollbar(
                trackColor:
                    Theme.of(context).colorScheme.surface.withAlpha(100),
                thumbColor: Theme.of(context).colorScheme.surfaceTint,
                radius: const Radius.circular(32.0),
                trackVisibility: true,
                thumbVisibility: true,
                thickness: 8.0,
                controller: _hScrollController,
                scrollbarOrientation: ScrollbarOrientation.bottom,
                child: SingleChildScrollView(
                  scrollDirection: Axis.horizontal,
                  controller: _hScrollController,
                  child: SingleChildScrollView(
                    controller: __vScrollController,
                    child: const SizedBox(
                      width: 4000,
                      height: 4000,
                      child: DrawAreaWidget(),
                    ),
                  ),
                ),
              ),
            ),
            Align(
              alignment: Alignment.topRight,
              child: RawScrollbar(
                trackColor:
                    Theme.of(context).colorScheme.surface.withAlpha(100),
                thumbColor: Theme.of(context).colorScheme.surfaceTint,
                radius: const Radius.circular(32.0),
                trackVisibility: true,
                thumbVisibility: true,
                thickness: 8.0,
                controller: _vScrollController,
                scrollbarOrientation: ScrollbarOrientation.right,
                child: SingleChildScrollView(
                  controller: _vScrollController,
                  child: const SizedBox(
                    height: 4000,
                    width: 8.0,
                  ),
                ),
              ),
            )
          ],
        ),
      ),
    );
  }
}

class DrawAreaWidget extends StatelessWidget {
  const DrawAreaWidget({
    super.key,
  });

  void onDrawAreaClick(BuildContext context, ProjectData data) {
    final mouseData = Provider.of<MouseData>(context, listen: false);
    if (data.drawingComponent != ComponentType.none) {
      ComponentGenerator.generate(
        data.drawingComponent,
        context,
        pos: mouseData.mousePos,
      );
      data.setDrawingComponent(ComponentType.none);
    } else if (data.selectedItemId != "") {
      if (data.components[data.selectedItemId] != null) {
        data.components[data.selectedItemId]!.properties.pos =
            mouseData.mousePos;
      }
      data.setSelectedItemId("");
    }
  }

  @override
  Widget build(BuildContext context) {
    final drawAreaData = Provider.of<ProjectData>(context);
    MouseData mouseData;
    if (drawAreaData.drawingComponent == ComponentType.none) {
      mouseData = Provider.of<MouseData>(context, listen: false);
    } else {
      mouseData = Provider.of<MouseData>(context);
    }
    return GestureDetector(
      onTap: () {
        onDrawAreaClick(context, drawAreaData);
      },
      child: MouseRegion(
        onHover: (e) {
          mouseData.setMousePos(e.localPosition);
        },
        child: Container(
          decoration: BoxDecoration(
            image: DecorationImage(
              colorFilter: ColorFilter.mode(
                Theme.of(context).colorScheme.surfaceVariant,
                BlendMode.srcOut,
              ),
              isAntiAlias: true,
              filterQuality: FilterQuality.high,
              image: const AssetImage("assets/images/grid.png"),
              repeat: ImageRepeat.repeat,
            ),
            color: Theme.of(context).colorScheme.surface,
            // color: Colors.transparent,
          ),
          child: Stack(
            children: [
              ...List.generate(
                drawAreaData.components.length,
                (index) {
                  if (index >= drawAreaData.components.length) {
                    return const SizedBox(width: 0, height: 0);
                  }
                  var item = drawAreaData.components.values.elementAt(index);
                  if (item.properties.type != ComponentType.pin) {
                    return item.draw(context);
                  } else {
                    return const SizedBox();
                  }
                },
              ),
              (drawAreaData.drawingComponent != ComponentType.none)
                  ? Positioned(
                      left: mouseData.mousePos.dx,
                      top: mouseData.mousePos.dy,
                      child: Text(
                        drawAreaData.drawingComponent.name.toLowerCase(),
                      ),
                    )
                  : const SizedBox()
            ],
          ),
        ),
      ),
    );
  }
}
