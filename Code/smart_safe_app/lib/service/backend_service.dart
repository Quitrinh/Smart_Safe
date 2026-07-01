import 'package:http/http.dart' as http;

class BackendService {
  static const String baseUrl =
      "https://smart-safe-api-etd9a7bsbhb6gyh8.southeastasia-01.azurewebsites.net";

  static Future<void> testBackend() async {
    try {
      final response = await http
          .get(Uri.parse("$baseUrl/"))
          .timeout(const Duration(seconds: 10));

      print("[TEST BACKEND STATUS] ${response.statusCode}");
      print("[TEST BACKEND BODY] ${response.body}");
    } catch (e) {
      print("[TEST BACKEND ERROR] $e");
    }
  }
}