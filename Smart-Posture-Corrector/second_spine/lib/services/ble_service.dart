import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import '../models/posture_data.dart';

class BleService extends ChangeNotifier {
  // BLE UUIDs
  static const String serviceUuid = '12345678-1234-1234-1234-123456789abc';
  static const String postureCharUuid =
      '12345678-1234-1234-1234-000000000001';
  static const String commandCharUuid =
      '12345678-1234-1234-1234-000000000002';
  static const String statusCharUuid =
      '12345678-1234-1234-1234-000000000003';

  BluetoothDevice? _device;
  BluetoothCharacteristic? _commandChar;
  BluetoothCharacteristic? _postureChar;
  BluetoothCharacteristic? _statusChar;

  StreamSubscription? _scanSub;
  StreamSubscription? _connectionSub;
  StreamSubscription? _notifySub;

  bool _isScanning = false;
  bool _isConnected = false;
  PostureData _lastData = PostureData.empty;
  final List<ScanResult> _scanResults = [];

  // Getters
  bool get isScanning => _isScanning;
  bool get isConnected => _isConnected;
  PostureData get lastData => _lastData;
  List<ScanResult> get scanResults => List.unmodifiable(_scanResults);
  BluetoothDevice? get connectedDevice => _device;

  // Stream for posture data
  final StreamController<PostureData> _postureController =
      StreamController<PostureData>.broadcast();
  Stream<PostureData> get postureStream => _postureController.stream;

  /// Request necessary Bluetooth permissions
  Future<bool> _requestPermissions() async {
    if (Platform.isAndroid) {
      // Android 12+ requires specific Bluetooth permissions
      // Older versions require location permission
      Map<Permission, PermissionStatus> statuses;
      
      try {
        statuses = await [
          Permission.bluetoothScan,
          Permission.bluetoothConnect,
          Permission.location,
        ].request();
        
        bool scanGranted = statuses[Permission.bluetoothScan]?.isGranted ?? false;
        bool connectGranted = statuses[Permission.bluetoothConnect]?.isGranted ?? false;
        bool locationGranted = statuses[Permission.location]?.isGranted ?? false;
        
        // It might be Android < 12 where bluetoothScan/Connect don't exist but location does,
        // so we check if the permission was requested and explicitly denied.
        bool hasScanAndConnect = statuses[Permission.bluetoothScan] != PermissionStatus.denied && 
                                 statuses[Permission.bluetoothConnect] != PermissionStatus.denied;
                                 
        return (scanGranted && connectGranted) || locationGranted || hasScanAndConnect;
      } catch (e) {
        debugPrint('Permission request error: $e');
        return false;
      }
    }
    // iOS usually handles these differently, but request() on iOS for Bluetooth can also be mapped
    // For now we assume iOS handles it or we rely on flutter_blue_plus's built-in prompts
    return true;
  }

  /// Start scanning for BLE devices
  Future<bool> startScan() async {
    if (_isScanning) return false;

    bool hasPermissions = await _requestPermissions();
    if (!hasPermissions) {
      debugPrint('Bluetooth permissions not granted');
      return false; // Could not get permissions
    }

    _scanResults.clear();
    _isScanning = true;
    notifyListeners();

    _scanSub = FlutterBluePlus.onScanResults.listen((results) {
      _scanResults.clear();
      _scanResults.addAll(results);
      notifyListeners();
    });

    await FlutterBluePlus.startScan(
      timeout: const Duration(seconds: 15),
    );

    _isScanning = false;
    notifyListeners();
    return true;
  }

  /// Stop scanning
  Future<void> stopScan() async {
    await FlutterBluePlus.stopScan();
    _scanSub?.cancel();
    _isScanning = false;
    notifyListeners();
  }

  /// Connect to a specific device
  Future<bool> connectToDevice(BluetoothDevice device) async {
    bool hasPermissions = await _requestPermissions();
    if (!hasPermissions) {
      debugPrint('Bluetooth permissions not granted');
      return false;
    }

    try {
      await device.connect(autoConnect: false);
      _device = device;
      _isConnected = true;

      // Listen for disconnection
      _connectionSub = device.connectionState.listen((state) {
        if (state == BluetoothConnectionState.disconnected) {
          _isConnected = false;
          _device = null;
          _commandChar = null;
          _postureChar = null;
          _statusChar = null;
          _notifySub?.cancel();
          notifyListeners();
        }
      });

      // Discover services and characteristics
      await _discoverServices(device);

      notifyListeners();
      return true;
    } catch (e) {
      debugPrint('BLE Connect error: $e');
      _isConnected = false;
      notifyListeners();
      return false;
    }
  }

  Future<void> _discoverServices(BluetoothDevice device) async {
    final services = await device.discoverServices();
    for (final service in services) {
      if (service.uuid.toString().toLowerCase() == serviceUuid) {
        for (final char in service.characteristics) {
          final uuid = char.uuid.toString().toLowerCase();
          if (uuid == postureCharUuid) {
            _postureChar = char;
            // Enable notifications
            await char.setNotifyValue(true);
            _notifySub = char.onValueReceived.listen(_onPostureData);
          } else if (uuid == commandCharUuid) {
            _commandChar = char;
          } else if (uuid == statusCharUuid) {
            _statusChar = char;
          }
        }
      }
    }
  }

  void _onPostureData(List<int> value) {
    try {
      final jsonStr = utf8.decode(value);
      final data = PostureData.fromJson(jsonStr);
      _lastData = data;
      _postureController.add(data);
      notifyListeners();
    } catch (e) {
      debugPrint('Parse error: $e');
    }
  }

  /// Send a command to the device
  Future<void> sendCommand(String command) async {
    if (_commandChar == null) return;
    try {
      await _commandChar!.write(utf8.encode(command), withoutResponse: false);
    } catch (e) {
      debugPrint('Command send error: $e');
    }
  }

  /// Convenience commands
  Future<void> calibrate() => sendCommand('CALIBRATE');
  Future<void> toggleMode() => sendCommand('TOGGLE_MODE');
  Future<void> setLevel(int level) => sendCommand('LEVEL:$level');

  /// Read current status
  Future<String?> readStatus() async {
    if (_statusChar == null) return null;
    try {
      final value = await _statusChar!.read();
      return utf8.decode(value);
    } catch (e) {
      debugPrint('Status read error: $e');
      return null;
    }
  }

  /// Disconnect
  Future<void> disconnect() async {
    _notifySub?.cancel();
    _connectionSub?.cancel();
    await _device?.disconnect();
    _device = null;
    _isConnected = false;
    _commandChar = null;
    _postureChar = null;
    _statusChar = null;
    notifyListeners();
  }

  @override
  void dispose() {
    _scanSub?.cancel();
    _connectionSub?.cancel();
    _notifySub?.cancel();
    _postureController.close();
    super.dispose();
  }
}
