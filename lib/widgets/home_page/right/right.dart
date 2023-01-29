import 'package:flutter/material.dart';

import './draw_area/draw_area.dart';

class Right extends StatelessWidget {
  const Right({
    Key? key,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return const SizedBox(
      height: double.maxFinite,
      child: const DrawArea(),
    );
  }
}
