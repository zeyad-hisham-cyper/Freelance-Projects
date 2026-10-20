import 'package:flutter/material.dart';

class AppColors {
  AppColors._();

  // Base
  static const Color background = Color(0xFF0A0E21);
  static const Color surface = Color(0xFF1A1F38);
  static const Color surfaceLight = Color(0xFF252A45);
  static const Color cardGlass = Color(0x33FFFFFF);

  // Accents
  static const Color cyan = Color(0xFF00E5FF);
  static const Color purple = Color(0xFFBB86FC);
  static const Color neonGreen = Color(0xFF00FF87);
  static const Color neonPink = Color(0xFFFF0080);
  static const Color amber = Color(0xFFFFAB40);

  // Status
  static const Color good = neonGreen;
  static const Color bad = neonPink;
  static const Color calibrating = amber;

  // Text
  static const Color textPrimary = Color(0xFFFFFFFF);
  static const Color textSecondary = Color(0xB3FFFFFF); // 70%
  static const Color textMuted = Color(0x80FFFFFF); // 50%

  // Gradients
  static const LinearGradient primaryGradient = LinearGradient(
    colors: [cyan, purple],
    begin: Alignment.topLeft,
    end: Alignment.bottomRight,
  );

  static const LinearGradient goodGradient = LinearGradient(
    colors: [Color(0xFF00FF87), Color(0xFF00E5FF)],
    begin: Alignment.topLeft,
    end: Alignment.bottomRight,
  );

  static const LinearGradient badGradient = LinearGradient(
    colors: [Color(0xFFFF0080), Color(0xFFFF6B35)],
    begin: Alignment.topLeft,
    end: Alignment.bottomRight,
  );

  static const LinearGradient cardGradient = LinearGradient(
    colors: [Color(0x1AFFFFFF), Color(0x0DFFFFFF)],
    begin: Alignment.topLeft,
    end: Alignment.bottomRight,
  );
}
