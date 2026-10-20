import 'dart:async';
import 'package:flutter/material.dart';
import '../models/posture_data.dart';
import '../services/ble_service.dart';
import '../services/storage_service.dart';
import '../theme/app_colors.dart';
import '../widgets/glass_card.dart';
import '../widgets/angle_gauge.dart';

class HomeScreen extends StatefulWidget {
  final BleService bleService;
  final StorageService storageService;

  const HomeScreen({
    super.key,
    required this.bleService,
    required this.storageService,
  });

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> with TickerProviderStateMixin {
  late AnimationController _pulseController;
  late Animation<double> _pulseAnimation;
  late AnimationController _calibrateController;
  late Animation<double> _calibrateAnimation;
  int _sensitivityLevel = 0;
  StreamSubscription? _postureSub;
  PostureData _data = PostureData.empty;

  @override
  void initState() {
    super.initState();
    _sensitivityLevel = widget.storageService.sensitivityLevel;

    _pulseController = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 1500),
    )..repeat(reverse: true);

    _pulseAnimation = Tween<double>(begin: 0.9, end: 1.1).animate(
      CurvedAnimation(parent: _pulseController, curve: Curves.easeInOut),
    );

    _calibrateController = AnimationController(
      vsync: this,
      duration: const Duration(seconds: 2),
    )..repeat();

    _calibrateAnimation = Tween<double>(begin: 0, end: 1).animate(
      CurvedAnimation(parent: _calibrateController, curve: Curves.linear),
    );

    _postureSub = widget.bleService.postureStream.listen((data) {
      if (mounted) setState(() => _data = data);
    });

    widget.bleService.addListener(_onBleUpdate);
  }

  void _onBleUpdate() {
    if (mounted) setState(() => _data = widget.bleService.lastData);
  }

  @override
  void dispose() {
    _pulseController.dispose();
    _calibrateController.dispose();
    _postureSub?.cancel();
    widget.bleService.removeListener(_onBleUpdate);
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final isConnected = widget.bleService.isConnected;

    return SingleChildScrollView(
      padding: const EdgeInsets.fromLTRB(20, 16, 20, 100),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          // Connection status
          _buildConnectionChip(isConnected),
          const SizedBox(height: 20),

          // Big posture status
          _buildPostureStatus(),
          const SizedBox(height: 24),

          // Angle gauges
          _buildGaugeRow(),
          const SizedBox(height: 24),

          // Mode & Calibration
          _buildModeSection(),
          const SizedBox(height: 20),

          // Sensitivity slider
          _buildSensitivitySection(),
        ],
      ),
    );
  }

  Widget _buildConnectionChip(bool connected) {
    return Center(
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
        decoration: BoxDecoration(
          color: connected
              ? AppColors.neonGreen.withAlpha(30)
              : AppColors.neonPink.withAlpha(30),
          borderRadius: BorderRadius.circular(20),
          border: Border.all(
            color: connected
                ? AppColors.neonGreen.withAlpha(80)
                : AppColors.neonPink.withAlpha(80),
          ),
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(
              connected ? Icons.bluetooth_connected : Icons.bluetooth_disabled,
              size: 16,
              color: connected ? AppColors.neonGreen : AppColors.neonPink,
            ),
            const SizedBox(width: 8),
            Text(
              connected ? 'Connected' : 'Disconnected',
              style: TextStyle(
                fontSize: 13,
                fontWeight: FontWeight.w600,
                color: connected ? AppColors.neonGreen : AppColors.neonPink,
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildPostureStatus() {
    final isBad = _data.isPostureBad;
    final isCalibrating = _data.calibrating;

    if (isCalibrating) {
      return GlassCard(
        padding: const EdgeInsets.symmetric(vertical: 32, horizontal: 20),
        child: Column(
          children: [
            AnimatedBuilder(
              animation: _calibrateAnimation,
              builder: (_, __) {
                return SizedBox(
                  width: 80,
                  height: 80,
                  child: CircularProgressIndicator(
                    value: null,
                    strokeWidth: 6,
                    color: AppColors.amber,
                    backgroundColor: AppColors.surfaceLight,
                  ),
                );
              },
            ),
            const SizedBox(height: 20),
            const Text(
              'Calibrating...',
              style: TextStyle(
                fontSize: 24,
                fontWeight: FontWeight.w700,
                color: AppColors.amber,
              ),
            ),
            const SizedBox(height: 8),
            const Text(
              'Sit upright and hold still',
              style: TextStyle(
                fontSize: 14,
                color: AppColors.textSecondary,
              ),
            ),
          ],
        ),
      );
    }

    return GlassCard(
      padding: const EdgeInsets.symmetric(vertical: 32, horizontal: 20),
      child: Column(
        children: [
          AnimatedBuilder(
            animation: _pulseAnimation,
            builder: (_, child) {
              return Transform.scale(
                scale: isBad ? _pulseAnimation.value : 1.0,
                child: child,
              );
            },
            child: Container(
              width: 90,
              height: 90,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                gradient: isBad ? AppColors.badGradient : AppColors.goodGradient,
                boxShadow: [
                  BoxShadow(
                    color: (isBad ? AppColors.neonPink : AppColors.neonGreen)
                        .withAlpha(80),
                    blurRadius: 30,
                    spreadRadius: 5,
                  ),
                ],
              ),
              child: Icon(
                isBad ? Icons.warning_rounded : Icons.check_rounded,
                size: 48,
                color: Colors.white,
              ),
            ),
          ),
          const SizedBox(height: 20),
          Text(
            isBad ? 'Fix Your Posture' : 'Great Posture!',
            style: TextStyle(
              fontSize: 26,
              fontWeight: FontWeight.w700,
              color: isBad ? AppColors.bad : AppColors.good,
            ),
          ),
          const SizedBox(height: 6),
          Text(
            isBad
                ? _buildBadPostureHint()
                : 'Keep it up — you\'re doing awesome',
            style: const TextStyle(
              fontSize: 14,
              color: AppColors.textSecondary,
            ),
          ),
        ],
      ),
    );
  }

  String _buildBadPostureHint() {
    if (_data.isBackBad && _data.isNeckBad) return 'Straighten your back & neck';
    if (_data.isBackBad) return 'Straighten your back';
    return 'Straighten your neck';
  }

  Widget _buildGaugeRow() {
    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceEvenly,
      children: [
        AngleGauge(
          label: 'Back',
          angle: _data.back,
          threshold: _data.backTh,
          isBad: _data.isBackBad,
        ),
        AngleGauge(
          label: 'Neck',
          angle: _data.neck,
          threshold: _data.neckTh,
          isBad: _data.isNeckBad,
        ),
      ],
    );
  }

  Widget _buildModeSection() {
    final isCalibrating = _data.mode == 0;

    return GlassCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Text(
            'Controls',
            style: TextStyle(
              fontSize: 16,
              fontWeight: FontWeight.w600,
              color: AppColors.textPrimary,
            ),
          ),
          const SizedBox(height: 16),
          Row(
            children: [
              // Mode toggle
              Expanded(
                child: _ActionButton(
                  icon: _data.mode == 2
                      ? Icons.self_improvement
                      : Icons.accessibility_new,
                  label: _data.modeString,
                  sublabel: 'Tap to toggle',
                  color: _data.mode == 2 ? AppColors.purple : AppColors.cyan,
                  onTap: isCalibrating
                      ? null
                      : () => widget.bleService.toggleMode(),
                ),
              ),
              const SizedBox(width: 12),
              // Calibrate button
              Expanded(
                child: _ActionButton(
                  icon: Icons.tune,
                  label: 'Calibrate',
                  sublabel: isCalibrating ? 'In progress...' : 'Tap to start',
                  color: AppColors.amber,
                  onTap: isCalibrating
                      ? null
                      : () => widget.bleService.calibrate(),
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }

  Widget _buildSensitivitySection() {
    return GlassCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              const Text(
                'Sensitivity',
                style: TextStyle(
                  fontSize: 16,
                  fontWeight: FontWeight.w600,
                  color: AppColors.textPrimary,
                ),
              ),
              Container(
                padding:
                    const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
                decoration: BoxDecoration(
                  gradient: AppColors.primaryGradient,
                  borderRadius: BorderRadius.circular(12),
                ),
                child: Text(
                  'Level $_sensitivityLevel',
                  style: const TextStyle(
                    fontSize: 13,
                    fontWeight: FontWeight.w700,
                    color: AppColors.background,
                  ),
                ),
              ),
            ],
          ),
          const SizedBox(height: 8),
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: const [
              Text('Lenient',
                  style: TextStyle(fontSize: 11, color: AppColors.textMuted)),
              Text('Strict',
                  style: TextStyle(fontSize: 11, color: AppColors.textMuted)),
            ],
          ),
          Slider(
            value: _sensitivityLevel.toDouble(),
            min: 0,
            max: 5,
            divisions: 5,
            onChanged: (val) {
              setState(() => _sensitivityLevel = val.round());
            },
            onChangeEnd: (val) async {
              final level = val.round();
              await widget.storageService.setSensitivityLevel(level);
              widget.bleService.setLevel(level);
            },
          ),
          const SizedBox(height: 4),
          Text(
            'Back ±${_data.backTh.toStringAsFixed(0)}°  •  Neck ±${_data.neckTh.toStringAsFixed(0)}°',
            style: const TextStyle(
              fontSize: 12,
              color: AppColors.textMuted,
            ),
          ),
        ],
      ),
    );
  }
}

class _ActionButton extends StatelessWidget {
  final IconData icon;
  final String label;
  final String sublabel;
  final Color color;
  final VoidCallback? onTap;

  const _ActionButton({
    required this.icon,
    required this.label,
    required this.sublabel,
    required this.color,
    this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    final enabled = onTap != null;
    return Material(
      color: Colors.transparent,
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(16),
        child: AnimatedOpacity(
          opacity: enabled ? 1.0 : 0.5,
          duration: const Duration(milliseconds: 200),
          child: Container(
            padding: const EdgeInsets.symmetric(vertical: 16, horizontal: 12),
            decoration: BoxDecoration(
              color: color.withAlpha(20),
              borderRadius: BorderRadius.circular(16),
              border: Border.all(color: color.withAlpha(60)),
            ),
            child: Column(
              children: [
                Icon(icon, color: color, size: 28),
                const SizedBox(height: 8),
                Text(
                  label,
                  style: TextStyle(
                    fontSize: 14,
                    fontWeight: FontWeight.w600,
                    color: color,
                  ),
                ),
                const SizedBox(height: 2),
                Text(
                  sublabel,
                  style: const TextStyle(
                    fontSize: 11,
                    color: AppColors.textMuted,
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
