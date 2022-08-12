import 'package:animated_text_kit/animated_text_kit.dart';
import 'package:bess/pages/home_page.dart';
import 'package:bess/themes.dart';
import 'package:bess/widgets/mwidgets/m_iconbutton.dart';
import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

class SplashPage extends StatelessWidget {
  const SplashPage({Key? key}) : super(key: key);

  static const String id = "splash_page_id";

  @override
  Widget build(BuildContext context) {
    return Scaffold(
        body: Container(
      color: MyTheme.backgroundColor,
      child: Padding(
        padding: const EdgeInsets.all(8.0),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            TextLiquidFill(
              text: "Best Electrical Simulation Software",
              boxBackgroundColor: MyTheme.backgroundColor,
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
            TextButton(
              onPressed: () {
                Navigator.popAndPushNamed(context, HomePage.id);
              },
              child: Row(
                mainAxisSize: MainAxisSize.min,
                children: const [
                  Text("Continue"),
                  Icon(Icons.arrow_forward_rounded)
                ],
              ),
            ),
          ],
        ),
      ),
    ));
  }
}
