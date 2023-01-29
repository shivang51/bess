import 'package:animated_text_kit/animated_text_kit.dart';
import 'package:bess/pages/home_page.dart';
import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

class SplashPage extends StatelessWidget {
  const SplashPage({Key? key}) : super(key: key);

  static const String id = "splash_page_id";

  @override
  Widget build(BuildContext context) {
    return Scaffold(
        body: Row(
      children: [
        Expanded(
          flex: 4,
          child: Padding(
            padding: const EdgeInsets.all(8.0),
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                const MAnimatedImage(),
                TextLiquidFill(
                  text: "Basic Electrical Simulation Software",
                  waveColor: Theme.of(context).textTheme.bodyLarge!.color!,
                  textAlign: TextAlign.center,
                  boxBackgroundColor: Theme.of(context).colorScheme.background,
                  waveDuration: const Duration(seconds: 1),
                  loadDuration: const Duration(seconds: 3),
                  textStyle: GoogleFonts.nunito(
                    textStyle: const TextStyle(
                      letterSpacing: 0.4,
                      fontSize: 48.0,
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                ),
              ],
            ),
          ),
        ),
        Expanded(
          flex: 6,
          child: Center(
            child: Card(
              child: Padding(
                padding: const EdgeInsets.all(8.0),
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Text(
                      "Your Projects",
                      style: Theme.of(context).textTheme.titleLarge!,
                    ),
                    const SizedBox(
                      height: 20.0,
                    ),
                    Text(
                      "No Projects Yet",
                      style: Theme.of(context).textTheme.titleSmall,
                    ),
                    const SizedBox(
                      height: 20.0,
                    ),
                    Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        ElevatedButton(
                          onPressed: () {},
                          child: const Text("New Project"),
                        ),
                        const SizedBox(
                          width: 20.0,
                        ),
                        ElevatedButton(
                          onPressed: () {
                            Navigator.popAndPushNamed(context, HomePage.id);
                          },
                          child: const Text("Continue with empty project"),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      ],
    ));
  }
}

class MAnimatedImage extends StatefulWidget {
  const MAnimatedImage({
    Key? key,
  }) : super(key: key);

  @override
  State<MAnimatedImage> createState() => _MAnimatedImageState();
}

class _MAnimatedImageState extends State<MAnimatedImage>
    with SingleTickerProviderStateMixin {
  late AnimationController _animationController;
  late Animation<Offset> _animation;
  @override
  void initState() {
    super.initState();
    _animationController = AnimationController(
        reverseDuration: const Duration(seconds: 1),
        duration: const Duration(seconds: 1),
        vsync: this);
    _animationController.addListener(() {
      if (_animationController.isCompleted) {
        _animationController.reverse();
      } else if (_animationController.isDismissed) {
        _animationController.forward();
      }

      setState(() {});
    });
    _animation = Tween<Offset>(
      begin: const Offset(-0.0, -20.0),
      end: const Offset(0.0, 10.0),
    ).animate(_animationController);
    _animationController.forward();
  }

  @override
  Widget build(BuildContext context) {
    return Transform(
      transform: Transform.translate(
        offset: Offset(0.0, _animation.value.dy),
      ).transform,
      child: Image.asset(
        "assets/images/app_icon.png",
        width: 250,
      ),
    );
  }

  @override
  void dispose() {
    _animationController.dispose();
    super.dispose();
  }
}
