import 'dart:math';
import 'package:flutter/material.dart';
import '../theme/app_colors.dart';

class AngleGauge extends StatelessWidget {
  final String label;
  final double angle;
  final double threshold;
  final bool isBad;

  const AngleGauge({
    super.key,
    required this.label,
    required this.angle,
    required this.threshold,
    required this.isBad,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        SizedBox(
          width: 130,
          height: 130,
          child: CustomPaint(
            painter: _GaugePainter(
              angle: angle,
              threshold: threshold,
              isBad: isBad,
            ),
            child: Center(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Text(
                    '${angle.toStringAsFixed(1)}°',
                    style: TextStyle(
                      fontSize: 22,
                      fontWeight: FontWeight.w700,
                      color: isBad ? AppColors.bad : AppColors.good,
                    ),
                  ),
                  const SizedBox(height: 2),
                  Text(
                    '±${threshold.toStringAsFixed(0)}°',
                    style: const TextStyle(
                      fontSize: 12,
                      color: AppColors.textMuted,
                    ),
                  ),
                ],
              ),
            ),
          ),
        ),
        const SizedBox(height: 8),
        Text(
          label,
          style: const TextStyle(
            fontSize: 14,
            fontWeight: FontWeight.w600,
            color: AppColors.textSecondary,
          ),
        ),
      ],
    );
  }
}

class _GaugePainter extends CustomPainter {
  final double angle;
  final double threshold;
  final bool isBad;

  _GaugePainter({
    required this.angle,
    required this.threshold,
    required this.isBad,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final radius = min(size.width, size.height) / 2 - 8;

    // Background arc
    final bgPaint = Paint()
      ..color = AppColors.surfaceLight
      ..style = PaintingStyle.stroke
      ..strokeWidth = 10
      ..strokeCap = StrokeCap.round;

    canvas.drawArc(
      Rect.fromCircle(center: center, radius: radius),
      -pi * 0.75,
      pi * 1.5,
      false,
      bgPaint,
    );

    // Progress arc
    final maxAngle = threshold * 2;
    final progress = (angle.abs() / maxAngle).clamp(0.0, 1.0);
    final sweepAngle = pi * 1.5 * progress;

    final progressPaint = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 10
      ..strokeCap = StrokeCap.round
      ..shader = LinearGradient(
        colors: isBad
            ? [AppColors.neonPink, AppColors.amber]
            : [AppColors.neonGreen, AppColors.cyan],
      ).createShader(Rect.fromCircle(center: center, radius: radius));

    canvas.drawArc(
      Rect.fromCircle(center: center, radius: radius),
      -pi * 0.75,
      sweepAngle,
      false,
      progressPaint,
    );

    // Glow effect when bad
    if (isBad) {
      final glowPaint = Paint()
        ..style = PaintingStyle.stroke
        ..strokeWidth = 16
        ..strokeCap = StrokeCap.round
        ..maskFilter = const MaskFilter.blur(BlurStyle.normal, 8)
        ..shader = LinearGradient(
          colors: [
            AppColors.neonPink.withAlpha(100),
            AppColors.amber.withAlpha(100),
          ],
        ).createShader(Rect.fromCircle(center: center, radius: radius));

      canvas.drawArc(
        Rect.fromCircle(center: center, radius: radius),
        -pi * 0.75,
        sweepAngle,
        false,
        glowPaint,
      );
    }
  }

  @override
  bool shouldRepaint(covariant _GaugePainter oldDelegate) =>
      oldDelegate.angle != angle ||
      oldDelegate.threshold != threshold ||
      oldDelegate.isBad != isBad;
}
