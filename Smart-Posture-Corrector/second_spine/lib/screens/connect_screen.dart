import 'package:flutter/material.dart';
import '../services/ble_service.dart';
import '../services/storage_service.dart';
import '../theme/app_colors.dart';
import '../widgets/glass_card.dart';

class ConnectScreen extends StatefulWidget {
  final BleService bleService;
  final StorageService storageService;
  final VoidCallback? onConnected;

  const ConnectScreen({
    super.key,
    required this.bleService,
    required this.storageService,
    this.onConnected,
  });

  @override
  State<ConnectScreen> createState() => _ConnectScreenState();
}

class _ConnectScreenState extends State<ConnectScreen>
    with SingleTickerProviderStateMixin {
  late AnimationController _radarController;
  bool _connecting = false;

  @override
  void initState() {
    super.initState();
    _radarController = AnimationController(
      vsync: this,
      duration: const Duration(seconds: 3),
    )..repeat();

    widget.bleService.addListener(_update);
  }

  void _update() {
    if (mounted) setState(() {});
  }

  @override
  void dispose() {
    _radarController.dispose();
    widget.bleService.removeListener(_update);
    super.dispose();
  }

  Future<void> _startScan() async {
    bool success = await widget.bleService.startScan();
    if (!success && mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('Bluetooth permissions are required to scan for devices'),
          backgroundColor: AppColors.bad,
        ),
      );
    }
  }

  Future<void> _connectToDevice(device) async {
    setState(() => _connecting = true);
    bool success = await widget.bleService.connectToDevice(device);

    if (!success && mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('Failed to connect or permissions omitted'),
          backgroundColor: AppColors.bad,
        ),
      );
    }

    if (widget.bleService.isConnected) {
      // Save device ID
      await widget.storageService
          .setLastDeviceId(device.remoteId.toString());

      // Send saved sensitivity level
      final level = widget.storageService.sensitivityLevel;
      widget.bleService.setLevel(level);

      widget.onConnected?.call();
    }

    if (mounted) setState(() => _connecting = false);
  }

  @override
  Widget build(BuildContext context) {
    final isConnected = widget.bleService.isConnected;
    final isScanning = widget.bleService.isScanning;
    final results = widget.bleService.scanResults;

    return SingleChildScrollView(
      padding: const EdgeInsets.fromLTRB(20, 16, 20, 100),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          // Status card
          if (isConnected) _buildConnectedCard() else _buildScanHeader(isScanning),
          const SizedBox(height: 24),

          if (!isConnected) ...[
            // Scan button
            if (!isScanning)
              Center(
                child: ElevatedButton.icon(
                  onPressed: _startScan,
                  icon: const Icon(Icons.bluetooth_searching),
                  label: const Text('Scan for Devices'),
                ),
              ),

            if (isScanning)
              Center(
                child: TextButton.icon(
                  onPressed: () => widget.bleService.stopScan(),
                  icon: const Icon(Icons.stop, color: AppColors.neonPink),
                  label: const Text(
                    'Stop Scanning',
                    style: TextStyle(color: AppColors.neonPink),
                  ),
                ),
              ),

            const SizedBox(height: 24),

            // Results
            if (results.isEmpty && !isScanning)
              _buildEmptyState()
            else
              ...results.map((r) => _buildDeviceCard(r)),
          ],

          if (isConnected)
            Padding(
              padding: const EdgeInsets.only(top: 24),
              child: OutlinedButton.icon(
                onPressed: () => widget.bleService.disconnect(),
                icon: const Icon(Icons.bluetooth_disabled,
                    color: AppColors.neonPink),
                label: const Text(
                  'Disconnect',
                  style: TextStyle(color: AppColors.neonPink),
                ),
                style: OutlinedButton.styleFrom(
                  side: const BorderSide(color: AppColors.neonPink),
                  padding: const EdgeInsets.symmetric(vertical: 14),
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(16),
                  ),
                ),
              ),
            ),
        ],
      ),
    );
  }

  Widget _buildScanHeader(bool scanning) {
    return GlassCard(
      child: Column(
        children: [
          SizedBox(
            width: 80,
            height: 80,
            child: scanning
                ? AnimatedBuilder(
                    animation: _radarController,
                    builder: (_, __) {
                      return Stack(
                        alignment: Alignment.center,
                        children: [
                          // Ripple rings
                          for (int i = 0; i < 3; i++)
                            _RadarRing(
                              controller: _radarController,
                              delay: i * 0.33,
                            ),
                          const Icon(
                            Icons.bluetooth_searching,
                            size: 32,
                            color: AppColors.cyan,
                          ),
                        ],
                      );
                    },
                  )
                : const Icon(
                    Icons.bluetooth,
                    size: 40,
                    color: AppColors.textMuted,
                  ),
          ),
          const SizedBox(height: 16),
          Text(
            scanning ? 'Scanning...' : 'Find Your Device',
            style: TextStyle(
              fontSize: 20,
              fontWeight: FontWeight.w700,
              color: scanning ? AppColors.cyan : AppColors.textPrimary,
            ),
          ),
          const SizedBox(height: 6),
          const Text(
            'Make sure your PostureCorrector is powered on',
            style: TextStyle(fontSize: 13, color: AppColors.textMuted),
          ),
        ],
      ),
    );
  }

  Widget _buildConnectedCard() {
    final device = widget.bleService.connectedDevice;
    return GlassCard(
      child: Column(
        children: [
          Container(
            width: 64,
            height: 64,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              gradient: AppColors.goodGradient,
              boxShadow: [
                BoxShadow(
                  color: AppColors.neonGreen.withAlpha(60),
                  blurRadius: 20,
                ),
              ],
            ),
            child: const Icon(
              Icons.bluetooth_connected,
              size: 32,
              color: Colors.white,
            ),
          ),
          const SizedBox(height: 16),
          const Text(
            'Connected',
            style: TextStyle(
              fontSize: 22,
              fontWeight: FontWeight.w700,
              color: AppColors.neonGreen,
            ),
          ),
          const SizedBox(height: 4),
          Text(
            device?.platformName ?? 'PostureCorrector',
            style: const TextStyle(
              fontSize: 14,
              color: AppColors.textMuted,
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildEmptyState() {
    return const Center(
      child: Padding(
        padding: EdgeInsets.only(top: 40),
        child: Column(
          children: [
            Icon(Icons.search_off, size: 48, color: AppColors.textMuted),
            SizedBox(height: 12),
            Text(
              'No devices found',
              style: TextStyle(
                fontSize: 16,
                color: AppColors.textMuted,
              ),
            ),
            SizedBox(height: 4),
            Text(
              'Tap "Scan" to search again',
              style: TextStyle(fontSize: 13, color: AppColors.textMuted),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildDeviceCard(scanResult) {
    final device = scanResult.device;
    final name = device.platformName.isNotEmpty
        ? device.platformName
        : scanResult.advertisementData.advName.isNotEmpty
            ? scanResult.advertisementData.advName
            : 'Unknown Device';
    final rssi = scanResult.rssi;

    return Padding(
      padding: const EdgeInsets.only(bottom: 10),
      child: GlassCard(
        padding: const EdgeInsets.all(16),
        child: ListTile(
          contentPadding: EdgeInsets.zero,
          leading: Container(
            width: 48,
            height: 48,
            decoration: BoxDecoration(
              color: AppColors.cyan.withAlpha(25),
              borderRadius: BorderRadius.circular(14),
            ),
            child: const Icon(
              Icons.devices,
              color: AppColors.cyan,
            ),
          ),
          title: Text(
            name,
            style: const TextStyle(
              fontWeight: FontWeight.w600,
              color: AppColors.textPrimary,
            ),
          ),
          subtitle: Text(
            'Signal: $rssi dBm',
            style: const TextStyle(
              fontSize: 12,
              color: AppColors.textMuted,
            ),
          ),
          trailing: _connecting
              ? const SizedBox(
                  width: 24,
                  height: 24,
                  child: CircularProgressIndicator(
                    strokeWidth: 2,
                    color: AppColors.cyan,
                  ),
                )
              : Container(
                  padding:
                      const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
                  decoration: BoxDecoration(
                    gradient: AppColors.primaryGradient,
                    borderRadius: BorderRadius.circular(12),
                  ),
                  child: const Text(
                    'Connect',
                    style: TextStyle(
                      fontSize: 13,
                      fontWeight: FontWeight.w600,
                      color: AppColors.background,
                    ),
                  ),
                ),
          onTap: _connecting ? null : () => _connectToDevice(device),
        ),
      ),
    );
  }
}

class _RadarRing extends StatelessWidget {
  final AnimationController controller;
  final double delay;

  const _RadarRing({required this.controller, required this.delay});

  @override
  Widget build(BuildContext context) {
    return AnimatedBuilder(
      animation: controller,
      builder: (_, __) {
        double value = (controller.value + delay) % 1.0;
        return Opacity(
          opacity: (1 - value).clamp(0.0, 0.6),
          child: Container(
            width: 80 * value,
            height: 80 * value,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              border: Border.all(
                color: AppColors.cyan,
                width: 2,
              ),
            ),
          ),
        );
      },
    );
  }
}
