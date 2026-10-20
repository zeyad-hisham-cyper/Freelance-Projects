import 'package:shared_preferences/shared_preferences.dart';
import '../models/session_record.dart';

class StorageService {
  static const String _sensitivityKey = 'sensitivity_level';
  static const String _sessionHistoryKey = 'session_history';
  static const String _lastDeviceIdKey = 'last_device_id';

  late SharedPreferences _prefs;

  Future<void> init() async {
    _prefs = await SharedPreferences.getInstance();
  }

  // Sensitivity Level
  int get sensitivityLevel => _prefs.getInt(_sensitivityKey) ?? 0;

  Future<void> setSensitivityLevel(int level) async {
    await _prefs.setInt(_sensitivityKey, level);
  }

  // Last Device ID
  String? get lastDeviceId => _prefs.getString(_lastDeviceIdKey);

  Future<void> setLastDeviceId(String id) async {
    await _prefs.setString(_lastDeviceIdKey, id);
  }

  // Session History
  List<SessionRecord> getSessionHistory() {
    final jsonStr = _prefs.getString(_sessionHistoryKey);
    if (jsonStr == null || jsonStr.isEmpty) return [];
    try {
      return SessionRecord.listFromJsonString(jsonStr);
    } catch (_) {
      return [];
    }
  }

  Future<void> saveSessionHistory(List<SessionRecord> records) async {
    final jsonStr = SessionRecord.listToJsonString(records);
    await _prefs.setString(_sessionHistoryKey, jsonStr);
  }

  Future<void> addSession(SessionRecord record) async {
    final history = getSessionHistory();
    history.add(record);
    // Keep last 50 sessions
    if (history.length > 50) {
      history.removeRange(0, history.length - 50);
    }
    await saveSessionHistory(history);
  }
}
