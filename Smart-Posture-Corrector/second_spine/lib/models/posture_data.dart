import 'dart:convert';

class PostureData {
  final double back;
  final double neck;
  final double backTh;
  final double neckTh;
  final int mode; // 0 = Calibrating, 1 = Normal, 2 = Special
  final bool calibrating;

  const PostureData({
    required this.back,
    required this.neck,
    required this.backTh,
    required this.neckTh,
    required this.mode,
    required this.calibrating,
  });

  factory PostureData.fromJson(String jsonString) {
    final map = json.decode(jsonString) as Map<String, dynamic>;
    return PostureData(
      back: (map['back'] as num).toDouble(),
      neck: (map['neck'] as num).toDouble(),
      backTh: (map['backTh'] as num).toDouble(),
      neckTh: (map['neckTh'] as num).toDouble(),
      mode: map['mode'] as int,
      calibrating: map['calibrating'] as bool,
    );
  }

  bool get isPostureBad => back.abs() > backTh || neck.abs() > neckTh;

  bool get isBackBad => back.abs() > backTh;
  bool get isNeckBad => neck.abs() > neckTh;

  String get modeString {
    switch (mode) {
      case 0:
        return 'Calibrating';
      case 1:
        return 'Normal';
      case 2:
        return 'Special';
      default:
        return 'Unknown';
    }
  }

  static const PostureData empty = PostureData(
    back: 0,
    neck: 0,
    backTh: 15,
    neckTh: 10,
    mode: 1,
    calibrating: false,
  );
}
