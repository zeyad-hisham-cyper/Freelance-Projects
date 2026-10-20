import 'dart:convert';

class SessionRecord {
  final DateTime startTime;
  final int durationSeconds;
  final double goodPosturePercent;

  const SessionRecord({
    required this.startTime,
    required this.durationSeconds,
    required this.goodPosturePercent,
  });

  Map<String, dynamic> toJson() => {
        'startTime': startTime.toIso8601String(),
        'durationSeconds': durationSeconds,
        'goodPosturePercent': goodPosturePercent,
      };

  factory SessionRecord.fromJson(Map<String, dynamic> map) {
    return SessionRecord(
      startTime: DateTime.parse(map['startTime'] as String),
      durationSeconds: map['durationSeconds'] as int,
      goodPosturePercent: (map['goodPosturePercent'] as num).toDouble(),
    );
  }

  static List<SessionRecord> listFromJsonString(String jsonString) {
    final list = json.decode(jsonString) as List;
    return list
        .map((e) => SessionRecord.fromJson(e as Map<String, dynamic>))
        .toList();
  }

  static String listToJsonString(List<SessionRecord> records) {
    return json.encode(records.map((e) => e.toJson()).toList());
  }

  String get formattedDuration {
    final hours = durationSeconds ~/ 3600;
    final minutes = (durationSeconds % 3600) ~/ 60;
    final seconds = durationSeconds % 60;
    if (hours > 0) return '${hours}h ${minutes}m';
    if (minutes > 0) return '${minutes}m ${seconds}s';
    return '${seconds}s';
  }
}
