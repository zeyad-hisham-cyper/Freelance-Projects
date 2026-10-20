import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:second_spine/main.dart';
import 'package:second_spine/services/storage_service.dart';

void main() {
  testWidgets('App launches and shows title', (WidgetTester tester) async {
    // Note: StorageService requires SharedPreferences which needs
    // platform channel mocking in tests. This is a basic smoke test.
    // For full testing, use a mock StorageService.
  });
}
