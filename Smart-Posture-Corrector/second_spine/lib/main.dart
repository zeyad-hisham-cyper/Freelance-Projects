import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'theme/app_theme.dart';
import 'theme/app_colors.dart';
import 'services/ble_service.dart';
import 'services/storage_service.dart';
import 'screens/home_screen.dart';
import 'screens/connect_screen.dart';
import 'screens/history_screen.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  // Lock to portrait
  await SystemChrome.setPreferredOrientations([DeviceOrientation.portraitUp]);

  // Transparent status bar
  SystemChrome.setSystemUIOverlayStyle(
    const SystemUiOverlayStyle(
      statusBarColor: Colors.transparent,
      statusBarIconBrightness: Brightness.light,
    ),
  );

  // Init storage
  final storageService = StorageService();
  await storageService.init();

  runApp(SecondSpineApp(storageService: storageService));
}

class SecondSpineApp extends StatelessWidget {
  final StorageService storageService;

  const SecondSpineApp({super.key, required this.storageService});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Second Spine',
      debugShowCheckedModeBanner: false,
      theme: AppTheme.darkTheme,
      home: AppShell(storageService: storageService),
    );
  }
}

class AppShell extends StatefulWidget {
  final StorageService storageService;

  const AppShell({super.key, required this.storageService});

  @override
  State<AppShell> createState() => _AppShellState();
}

class _AppShellState extends State<AppShell> {
  final BleService _bleService = BleService();
  int _currentIndex = 0;

  @override
  void dispose() {
    _bleService.dispose();
    super.dispose();
  }

  void _onConnected() {
    // Switch to home tab after connecting
    setState(() => _currentIndex = 0);
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      extendBody: true,
      appBar: AppBar(
        title: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            ShaderMask(
              shaderCallback: (rect) =>
                  AppColors.primaryGradient.createShader(rect),
              child: const Icon(
                Icons.straighten,
                color: Colors.white,
                size: 24,
              ),
            ),
            const SizedBox(width: 10),
            ShaderMask(
              shaderCallback: (rect) =>
                  AppColors.primaryGradient.createShader(rect),
              child: const Text(
                'Second Spine',
                style: TextStyle(
                  fontSize: 22,
                  fontWeight: FontWeight.w700,
                  color: Colors.white,
                ),
              ),
            ),
          ],
        ),
      ),
      body: IndexedStack(
        index: _currentIndex,
        children: [
          HomeScreen(
            bleService: _bleService,
            storageService: widget.storageService,
          ),
          ConnectScreen(
            bleService: _bleService,
            storageService: widget.storageService,
            onConnected: _onConnected,
          ),
          HistoryScreen(storageService: widget.storageService),
        ],
      ),
      bottomNavigationBar: Container(
        decoration: BoxDecoration(
          color: AppColors.surface,
          border: Border(
            top: BorderSide(color: Colors.white.withAlpha(13), width: 1),
          ),
        ),
        child: BottomNavigationBar(
          currentIndex: _currentIndex,
          onTap: (i) => setState(() => _currentIndex = i),
          items: const [
            BottomNavigationBarItem(
              icon: Icon(Icons.dashboard_rounded),
              activeIcon: Icon(Icons.dashboard_rounded),
              label: 'Dashboard',
            ),
            BottomNavigationBarItem(
              icon: Icon(Icons.bluetooth_rounded),
              activeIcon: Icon(Icons.bluetooth_connected),
              label: 'Connect',
            ),
            BottomNavigationBarItem(
              icon: Icon(Icons.history_rounded),
              activeIcon: Icon(Icons.history_rounded),
              label: 'History',
            ),
          ],
        ),
      ),
    );
  }
}
