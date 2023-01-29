import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

class BottomBar extends StatelessWidget {
  const BottomBar({super.key});

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisAlignment: MainAxisAlignment.end,
      children: [
        // const ScaleSlider(),
      ],
    );
  }
}

class ScaleSlider extends StatefulWidget {
  const ScaleSlider({
    super.key,
  });

  @override
  State<ScaleSlider> createState() => _ScaleSliderState();
}

class _ScaleSliderState extends State<ScaleSlider> {
  final double _kSliderMin = -2.0;
  final double _kSliderMax = 2.0;
  double _value = 0.0;

  @override
  Widget build(BuildContext context) {
    DrawAreaData _drawAreaData = Provider.of<DrawAreaData>(context);
    _value = _drawAreaData.scaleValue - 3;
    return Row(
      children: [
        Text(_value.toStringAsPrecision(3)),
        Slider(
          divisions: 16,
          label: _value.toString(),
          max: _kSliderMax,
          min: _kSliderMin,
          value: _value,
          onChanged: (value) => _drawAreaData.setScale(value + 3),
        ),
      ],
    );
  }
}
