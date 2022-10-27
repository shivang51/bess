import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

class MyTheme {
  static Color backgroundColor = Colors.grey[850]!;
  static Color primaryBgColor = Colors.grey[800]!;
  static Color secondaryBgColor = Colors.grey;
  static Color highlightColor = backgroundColor;
  static Color primaryTextColor = Colors.white;
  static Color secondaryTextColor = Colors.white60;
  static Color invertedTextColor = Colors.black87;
  static double primaryBorderRadius = 15.0;
  static Color primarySelectionColor = secondaryBgColor;
  static Color borderColor = Colors.white70;
}

var mTextButtonThemeData = TextButtonThemeData(
  style: TextButton.styleFrom(
    foregroundColor: MyTheme.primaryTextColor,
    backgroundColor: MyTheme.highlightColor,
    textStyle: GoogleFonts.roboto(
      textStyle: const TextStyle(
        letterSpacing: 0.4,
      ),
    ),
    shape: const StadiumBorder(),
  ),
);

var mCardTheme = CardTheme(
  shape: const StadiumBorder(),
  color: MyTheme.secondaryBgColor,
  margin: EdgeInsets.zero,
);

var mListTileThemeData = ListTileThemeData(
  shape: const StadiumBorder(),
  tileColor: MyTheme.backgroundColor,
  contentPadding: EdgeInsets.zero,
  dense: true,
);
