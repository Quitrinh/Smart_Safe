import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'dart:async';
import 'package:url_launcher/url_launcher.dart';
const String baseUrl = "https://smart-safe-api-etd9a7bsbhb6gyh8.southeastasia-01.azurewebsites.net";

void main() {
  runApp(const SmartSafeApp());
}

class SmartSafeApp extends StatelessWidget {
  const SmartSafeApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: "Smart Safe",
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorSchemeSeed: Colors.teal,
        useMaterial3: true,
        fontFamily: "Arial",
      ),
      home: const LoginPage(),
    );
  }
}

class LoginPage extends StatefulWidget {
  const LoginPage({super.key});

  @override
  State<LoginPage> createState() => _LoginPageState();
}

class _LoginPageState extends State<LoginPage> {
  final usernameCtrl = TextEditingController(text: "admin");
  final passwordCtrl = TextEditingController(text: "123456");

  bool loading = false;
  String error = "";

  Future<void> login() async {
    setState(() {
      loading = true;
      error = "";
    });

    try {
      final res = await http.post(
        Uri.parse("$baseUrl/api/login"),
        headers: {"Content-Type": "application/json"},
        body: jsonEncode({
          "username": usernameCtrl.text.trim(),
          "password": passwordCtrl.text.trim(),
        }),
      );

      final data = jsonDecode(res.body);

      if (data["success"] == true) {
        if (!mounted) return;

        Navigator.pushReplacement(
          context,
          MaterialPageRoute(
            builder: (_) => DashboardPage(
              userId: data["user"]["id"],
              username: data["user"]["username"],
              role: data["user"]["role"],
            ),
          ),
        );
      } else {
        setState(() {
          error = data["message"] ?? "Đăng nhập thất bại";
        });
      }
    } catch (_) {
      setState(() {
        error = "Không kết nối được backend";
      });
    }

    setState(() {
      loading = false;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xffe8f5f3),
      body: Center(
        child: Container(
          width: 390,
          padding: const EdgeInsets.all(28),
          decoration: BoxDecoration(
            color: Colors.white,
            borderRadius: BorderRadius.circular(30),
            boxShadow: [
              BoxShadow(
                color: Colors.teal.withOpacity(0.15),
                blurRadius: 30,
                offset: const Offset(0, 12),
              ),
            ],
          ),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Container(
                width: 92,
                height: 92,
                decoration: BoxDecoration(
                  color: Colors.teal.withOpacity(0.12),
                  shape: BoxShape.circle,
                ),
                child: const Icon(Icons.lock, size: 56, color: Colors.teal),
              ),
              const SizedBox(height: 18),
              const Text(
                "SMART SAFE",
                style: TextStyle(fontSize: 28, fontWeight: FontWeight.bold),
              ),
              const SizedBox(height: 6),
              const Text("Hệ thống két thông minh"),
              const SizedBox(height: 26),
              TextField(
                controller: usernameCtrl,
                decoration: const InputDecoration(
                  labelText: "Tài khoản",
                  prefixIcon: Icon(Icons.person),
                  border: OutlineInputBorder(),
                ),
              ),
              const SizedBox(height: 14),
              TextField(
                controller: passwordCtrl,
                obscureText: true,
                decoration: const InputDecoration(
                  labelText: "Mật khẩu",
                  prefixIcon: Icon(Icons.password),
                  border: OutlineInputBorder(),
                ),
              ),
              const SizedBox(height: 12),
              if (error.isNotEmpty)
                Text(error, style: const TextStyle(color: Colors.red)),
              const SizedBox(height: 18),
              SizedBox(
                width: double.infinity,
                height: 50,
                child: FilledButton.icon(
                  onPressed: loading ? null : login,
                  icon: const Icon(Icons.login),
                  label: loading
                      ? const CircularProgressIndicator(color: Colors.white)
                      : const Text("Đăng nhập"),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class DashboardPage extends StatefulWidget {
  final int userId;
  final String username;
  final String role;

  const DashboardPage({
    super.key,
    required this.userId,
    required this.username,
    required this.role,
  });

  @override
  State<DashboardPage> createState() => _DashboardPageState();
}

class _DashboardPageState extends State<DashboardPage> {
  int tabIndex = 0;
  List authMethods = [];
  Map<String, dynamic>? safeStatus;
  List events = [];
  List smsReceivers = [];
  List configs = [];

  bool loading = false;

  Timer? statusTimer;

  @override
  void initState() {
    super.initState();

    refreshAll();

    statusTimer = Timer.periodic(
      const Duration(seconds: 2),
      (_) {
        fetchSafeStatus();
      },
    );
  }

  @override
  void dispose() {
    statusTimer?.cancel();
    super.dispose();
  }

  Future<void> refreshAll() async {
    await fetchSafeStatus();
    await fetchEvents();
    await fetchSmsReceivers();
    await fetchConfig();
    await fetchAuthMethods();
  }

  Future<void> fetchSafeStatus() async {
    try {
      final res = await http.get(Uri.parse("$baseUrl/api/safe/status"));
      final data = jsonDecode(res.body);
      if (data["success"] == true) {
        setState(() => safeStatus = data["data"]);
      }
    } catch (_) {}
  }

  Future<void> fetchEvents() async {
    setState(() => loading = true);
    try {
      final res = await http.get(Uri.parse("$baseUrl/api/events"));
      final data = jsonDecode(res.body);
      if (data["success"] == true) {
        setState(() => events = data["data"]);
      }
    } catch (_) {}
    setState(() => loading = false);
  }

  Future<void> fetchSmsReceivers() async {
    try {
      final res = await http.get(Uri.parse("$baseUrl/api/sms-receivers"));
      final data = jsonDecode(res.body);
      if (data["success"] == true) {
        setState(() => smsReceivers = data["data"]);
      }
    } catch (_) {}
  }

  Future<void> fetchConfig() async {
    try {
      final res = await http.get(Uri.parse("$baseUrl/api/config"));
      final data = jsonDecode(res.body);
      if (data["success"] == true) {
        setState(() => configs = data["data"]);
      }
    } catch (_) {}
  }

  String configValue(String key) {
    for (final c in configs) {
      if (c["config_key"] == key) return c["config_value"].toString();
    }
    return "";
  }

  Future<void> requestOtp() async {
    final res = await http.post(
      Uri.parse("$baseUrl/api/request-open-otp"),
      headers: {"Content-Type": "application/json"},
      body: jsonEncode({"user_id": widget.userId}),
    );

    final data = jsonDecode(res.body);

    if (data["success"] == true && mounted) {
      showDialog(
        context: context,
        builder: (_) => OtpDialog(
          userId: widget.userId,
          otp: data["otp"].toString(),
          onSuccess: refreshAll,
        ),
      );
    }
  }

  Future<void> addSmsReceiver() async {
    final nameCtrl = TextEditingController();
    final phoneCtrl = TextEditingController();

    showDialog(
      context: context,
      builder: (_) => AlertDialog(
        title: const Text("Thêm số nhận SMS"),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(controller: nameCtrl, decoration: const InputDecoration(labelText: "Tên")),
            TextField(controller: phoneCtrl, decoration: const InputDecoration(labelText: "Số điện thoại")),
          ],
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(context), child: const Text("Hủy")),
          FilledButton(
            onPressed: () async {
              await http.post(
                Uri.parse("$baseUrl/api/sms-receivers"),
                headers: {"Content-Type": "application/json"},
                body: jsonEncode({
                  "name": nameCtrl.text.trim(),
                  "phone": phoneCtrl.text.trim(),
                }),
              );
              if (!mounted) return;
              Navigator.pop(context);
              fetchSmsReceivers();
            },
            child: const Text("Lưu"),
          ),
        ],
      ),
    );
  }

  Future<void> deleteSmsReceiver(int id) async {
    await http.delete(Uri.parse("$baseUrl/api/sms-receivers/$id"));
    fetchSmsReceivers();
  }

  Future<void> changeAdminPassword() async {
    final oldCtrl = TextEditingController();
    final newCtrl = TextEditingController();

    showDialog(
      context: context,
      builder: (_) => AlertDialog(
        title: const Text("Đổi mật khẩu admin"),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: oldCtrl,
              obscureText: true,
              decoration: const InputDecoration(labelText: "Mật khẩu cũ"),
            ),
            TextField(
              controller: newCtrl,
              obscureText: true,
              decoration: const InputDecoration(labelText: "Mật khẩu mới"),
            ),
          ],
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(context), child: const Text("Hủy")),
          FilledButton(
            onPressed: () async {
              final res = await http.post(
                Uri.parse("$baseUrl/api/change-password"),
                headers: {"Content-Type": "application/json"},
                body: jsonEncode({
                  "user_id": widget.userId,
                  "old_password": oldCtrl.text.trim(),
                  "new_password": newCtrl.text.trim(),
                }),
              );

              final data = jsonDecode(res.body);

              if (!mounted) return;
              Navigator.pop(context);

              ScaffoldMessenger.of(context).showSnackBar(
                SnackBar(content: Text(data["message"] ?? "Đã xử lý")),
              );
            },
            child: const Text("Đổi"),
          ),
        ],
      ),
    );
  }

  Future<void> changeKeypadPassword() async {
    final keypassCtrl = TextEditingController(text: configValue("keypad_password"));

    showDialog(
      context: context,
      builder: (_) => AlertDialog(
        title: const Text("Đổi mật khẩu keypad"),
        content: TextField(
          controller: keypassCtrl,
          keyboardType: TextInputType.number,
          decoration: const InputDecoration(
            labelText: "Mật khẩu keypad mới",
            hintText: "Ví dụ: 123456",
          ),
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(context), child: const Text("Hủy")),
          FilledButton(
            onPressed: () async {
              await http.post(
                Uri.parse("$baseUrl/api/config"),
                headers: {"Content-Type": "application/json"},
                body: jsonEncode({
                  "config_key": "keypad_password",
                  "config_value": keypassCtrl.text.trim(),
                }),
              );

              if (!mounted) return;
              Navigator.pop(context);
              fetchConfig();

              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(content: Text("Đã cập nhật mật khẩu keypad")),
              );
            },
            child: const Text("Lưu"),
          ),
        ],
      ),
    );
  }

  Future<void> changeWiFiConfig() async {
    final ssidCtrl = TextEditingController(
      text: configValue("wifi_ssid"),
    );

    final passCtrl = TextEditingController(
      text: configValue("wifi_password"),
    );

    showDialog(
      context: context,
      builder: (_) => AlertDialog(
        title: const Text("Cấu hình WiFi"),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: ssidCtrl,
              decoration: const InputDecoration(
                labelText: "Tên WiFi",
                prefixIcon: Icon(Icons.wifi),
              ),
            ),
            TextField(
              controller: passCtrl,
              obscureText: true,
              decoration: const InputDecoration(
                labelText: "Mật khẩu WiFi",
                prefixIcon: Icon(Icons.password),
              ),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text("Hủy"),
          ),
          FilledButton(
            onPressed: () async {
              await http.post(
                Uri.parse("$baseUrl/api/config"),
                headers: {"Content-Type": "application/json"},
                body: jsonEncode({
                  "config_key": "wifi_ssid",
                  "config_value": ssidCtrl.text.trim(),
                }),
              );

              await http.post(
                Uri.parse("$baseUrl/api/config"),
                headers: {"Content-Type": "application/json"},
                body: jsonEncode({
                  "config_key": "wifi_password",
                  "config_value": passCtrl.text.trim(),
                }),
              );

              if (!mounted) return;

              Navigator.pop(context);
              fetchConfig();

              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(
                  content: Text("Đã lưu cấu hình WiFi"),
                ),
              );
            },
            child: const Text("Lưu"),
          ),
        ],
      ),
    );
  }
Future<void> openMap(dynamic lat, dynamic lng) async {
    if (lat == null || lng == null) return;

    final url = Uri.parse(
      "https://maps.google.com/?q=$lat,$lng",
    );

    await launchUrl(
      url,
      mode: LaunchMode.externalApplication,
    );
  }

Future<void> fetchAuthMethods() async {
  try {
    final res = await http.get(
      Uri.parse("$baseUrl/api/auth-methods"),
    );

    final data = jsonDecode(res.body);

    if (data["success"] == true) {
      setState(() {
        authMethods = data["data"];
      });
    }
  } catch (_) {}
}

Future<void> enrollAuthMethod(String methodType) async {
  final passCtrl = TextEditingController();
  final nameCtrl = TextEditingController();

  showDialog(
    context: context,
    builder: (_) => AlertDialog(
      title: Text(
        methodType == "RFID" ? "Thêm thẻ RFID" : "Thêm vân tay",
      ),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          TextField(
            controller: passCtrl,
            obscureText: true,
            decoration: const InputDecoration(
              labelText: "Mật khẩu admin",
              prefixIcon: Icon(Icons.lock),
            ),
          ),
          const SizedBox(height: 12),
          TextField(
            controller: nameCtrl,
            decoration: const InputDecoration(
              labelText: "Tên người dùng",
              prefixIcon: Icon(Icons.person),
            ),
          ),
        ],
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: const Text("Hủy"),
        ),
        FilledButton(
          onPressed: () async {
            final verify = await http.post(
              Uri.parse("$baseUrl/api/admin/verify-password"),
              headers: {"Content-Type": "application/json"},
              body: jsonEncode({
                "password": passCtrl.text.trim(),
              }),
            );

            final verifyData = jsonDecode(verify.body);

            if (verifyData["success"] != true) {
              if (!mounted) return;
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(content: Text("Sai mật khẩu admin")),
              );
              return;
            }

            await http.post(
              Uri.parse("$baseUrl/api/auth-methods/enroll"),
              headers: {"Content-Type": "application/json"},
              body: jsonEncode({
                "user_name": nameCtrl.text.trim(),
                "method_type": methodType,
              }),
            );

            if (!mounted) return;

            Navigator.pop(context);

            ScaffoldMessenger.of(context).showSnackBar(
              SnackBar(
                content: Text(
                  methodType == "RFID"
                      ? "Hãy quét thẻ RFID trên két"
                      : "Hãy đặt tay lên cảm biến vân tay",
                ),
              ),
            );
          },
          child: const Text("Bắt đầu"),
        ),
      ],
    ),
  );
}

  @override
  Widget build(BuildContext context) {
    final pages = [
      dashboardView(),
      eventsView(),
      smsView(),
      settingsView(),
      authMethodsView(),
    ];

    return Scaffold(
      backgroundColor: const Color(0xfff2faf8),
      appBar: AppBar(
        title: const Text("Smart Safe"),
        actions: [
          IconButton(onPressed: refreshAll, icon: const Icon(Icons.refresh)),
          IconButton(
            icon: const Icon(Icons.logout),
            onPressed: () {
              Navigator.pushReplacement(
                context,
                MaterialPageRoute(builder: (_) => const LoginPage()),
              );
            },
          ),
        ],
      ),
      body: pages[tabIndex],
      bottomNavigationBar: NavigationBar(
        selectedIndex: tabIndex,
        onDestinationSelected: (i) => setState(() => tabIndex = i),
        destinations: const [
          NavigationDestination(icon: Icon(Icons.dashboard), label: "Tổng quan"),
          NavigationDestination(icon: Icon(Icons.history), label: "Lịch sử"),
          NavigationDestination(icon: Icon(Icons.sms), label: "SMS"),
          NavigationDestination(icon: Icon(Icons.settings), label: "Cài đặt"),
          NavigationDestination(icon: Icon(Icons.fingerprint), label: "Xác thực"),
        ],
      ),
    );
  }

  Widget dashboardView() {
    return RefreshIndicator(
      onRefresh: refreshAll,
      child: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Container(
            padding: const EdgeInsets.all(22),
            decoration: BoxDecoration(
              gradient: const LinearGradient(
                colors: [Color(0xff009688), Color(0xff4db6ac)],
              ),
              borderRadius: BorderRadius.circular(28),
            ),
            child: Row(
              children: [
                const Icon(Icons.security, color: Colors.white, size: 54),
                const SizedBox(width: 16),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      const Text(
                        "SMART SAFE ONLINE",
                        style: TextStyle(color: Colors.white, fontSize: 20, fontWeight: FontWeight.bold),
                      ),
                      const SizedBox(height: 6),
                      Text(
                        "Xin chào, ${widget.username} • ${widget.role}",
                        style: const TextStyle(color: Colors.white),
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(height: 18),
          GridView.count(
            shrinkWrap: true,
            physics: const NeverScrollableScrollPhysics(),
            crossAxisCount: 2,
            childAspectRatio: 1.15,
            mainAxisSpacing: 12,
            crossAxisSpacing: 12,
            children: [
              statusCard("Két", safeStatus?["safe_state"] ?? "UNKNOWN", Icons.lock, Colors.teal),
              statusCard("WiFi", safeStatus?["wifi_status"] ?? "UNKNOWN", Icons.wifi, Colors.blue),
              statusCard("SIM 4G", safeStatus?["sim_status"] ?? "UNKNOWN", Icons.sim_card, Colors.orange),
              statusCard("GPS", safeStatus?["gps_status"] ?? "UNKNOWN", Icons.location_on, Colors.pink),
              statusCard("Alarm", safeStatus?["alarm_status"] ?? "UNKNOWN", Icons.warning, Colors.red),
              statusCard("Flame", safeStatus?["flame_status"] ?? "UNKNOWN", Icons.local_fire_department, Colors.deepOrange),
              statusCard("Pump", safeStatus?["pump_status"] ?? "UNKNOWN", Icons.water_drop, Colors.cyan),
            ],
          ),
          const SizedBox(height: 22),
          SizedBox(
            height: 54,
            child: FilledButton.icon(
              onPressed: requestOtp,
              icon: const Icon(Icons.lock_open),
              label: const Text("Mở két bằng OTP"),
            ),
          ),
        ],
      ),
    );
  }

  Widget eventsView() {
    return RefreshIndicator(
      onRefresh: fetchEvents,
      child: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          title("Lịch sử sự kiện"),
          if (loading)
            const Center(child: CircularProgressIndicator())
          else if (events.isEmpty)
            const Text("Chưa có sự kiện")
          else
            ...events.map(
              (e) => Card(
                child: ListTile(
                  leading: Icon(eventIcon(e["event_type"]?.toString() ?? ""), color: eventColor(e["event_type"]?.toString() ?? "")),
                  title: Text(e["event_type"]?.toString() ?? ""),
                  subtitle: Text(
                  "${e["message"] ?? ""}\n${formatTime(e["created_at"]?.toString())}",
                  ),
                 // trailing: Text(e["network_type"]?.toString() ?? ""),
                  trailing: IconButton(
                      icon: const Icon(Icons.map, color: Colors.teal),
                      onPressed: () {
                        openMap(
                          e["gps_lat"],
                          e["gps_lng"],
                        );
                      },
                    ),
                  isThreeLine: true,
                ),
              ),
            ),
        ],
      ),
    );
  }

  Widget smsView() {
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        title("Số điện thoại nhận SMS"),
        FilledButton.icon(
          onPressed: addSmsReceiver,
          icon: const Icon(Icons.add),
          label: const Text("Thêm số SMS"),
        ),
        const SizedBox(height: 12),
        if (smsReceivers.isEmpty)
          const Text("Chưa có số SMS")
        else
          ...smsReceivers.map(
            (s) => Card(
              child: ListTile(
                leading: const Icon(Icons.phone, color: Colors.teal),
                title: Text(s["name"]?.toString() ?? ""),
                subtitle: Text(s["phone"]?.toString() ?? ""),
                trailing: IconButton(
                  icon: const Icon(Icons.delete, color: Colors.red),
                  onPressed: () => deleteSmsReceiver(s["id"]),
                ),
              ),
            ),
          ),
      ],
    );
  }

  Widget settingsView() {
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        title("Cài đặt hệ thống"),
        Card(
          child: ListTile(
            leading: const Icon(Icons.password, color: Colors.teal),
            title: const Text("Đổi mật khẩu admin"),
            subtitle: const Text("Thay đổi mật khẩu đăng nhập ứng dụng"),
            trailing: const Icon(Icons.chevron_right),
            onTap: changeAdminPassword,
          ),
        ),
        Card(
          child: ListTile(
            leading: const Icon(Icons.pin, color: Colors.orange),
            title: const Text("Đổi mật khẩu keypad"),
            subtitle: Text("Hiện tại: ${configValue("keypad_password").isEmpty ? "Chưa cấu hình" : configValue("keypad_password")}"),
            trailing: const Icon(Icons.chevron_right),
            onTap: changeKeypadPassword,
          ),
        ),
        Card(
          child: ListTile(
            leading: const Icon(Icons.settings, color: Colors.blue),
            title: const Text("Cấu hình cảnh báo"),
            subtitle: Text("Max wrong password: ${configValue("max_wrong_password")}"),
          ),
        ),
        Card(
          child: ListTile(
            leading: const Icon(Icons.wifi, color: Colors.blue),
            title: const Text("Cấu hình WiFi"),
            subtitle: Text(
              "SSID: ${configValue("wifi_ssid").isEmpty ? "Chưa cấu hình" : configValue("wifi_ssid")}",
            ),
            trailing: const Icon(Icons.chevron_right),
            onTap: changeWiFiConfig,
        ),
      ),      
      ],
    );
  }

  Widget title(String text) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 12),
      child: Text(text, style: const TextStyle(fontSize: 22, fontWeight: FontWeight.bold)),
    );
  }

  Widget statusCard(String title, String value, IconData icon, Color color) {
    return Container(
      padding: const EdgeInsets.all(18),
      decoration: BoxDecoration(
        color: color.withOpacity(0.12),
        borderRadius: BorderRadius.circular(24),
      ),
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Icon(icon, color: color, size: 38),
          const SizedBox(height: 8),
          Text(title),
          const SizedBox(height: 6),
          Text(value, style: TextStyle(color: color, fontWeight: FontWeight.bold, fontSize: 18)),
        ],
      ),
    );
  }
  Widget authMethodsView() {
    return RefreshIndicator(
      onRefresh: fetchAuthMethods,
      child: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          const Text(
            "Quản lý RFID / Vân tay",
            style: TextStyle(
              fontSize: 24,
              fontWeight: FontWeight.bold,
            ),
          ),

          const SizedBox(height: 16),

          FilledButton.icon(
            onPressed: () => enrollAuthMethod("RFID"),
            icon: const Icon(Icons.credit_card),
            label: const Text("Thêm RFID"),
          ),

          const SizedBox(height: 10),

          FilledButton.icon(
            onPressed: () => enrollAuthMethod("FINGERPRINT"),
            icon: const Icon(Icons.fingerprint),
            label: const Text("Thêm vân tay"),
          ),

          const SizedBox(height: 20),

          if (authMethods.isEmpty)
            const Text("Chưa có phương thức xác thực")
          else
            ...authMethods.map(
              (a) {
                final type = a["method_type"]?.toString() ?? "";
                final value = a["method_value"]?.toString() ?? "";
                final name = a["user_name"]?.toString() ?? "";

                return Card(
                  child: ListTile(
                    leading: CircleAvatar(
                      backgroundColor: Colors.teal.withOpacity(0.12),
                      child: Icon(
                        type == "RFID"
                            ? Icons.credit_card
                            : Icons.fingerprint,
                        color: Colors.teal,
                      ),
                    ),
                    title: Text(
                      name,
                      style: const TextStyle(
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                    subtitle: Text(
                      type == "RFID"
                          ? "Thẻ RFID: $value"
                          : "Vân tay ID: $value",
                    ),
                  ),
                );
              },
            ),
        ],
      ),
    );
  }
  
  IconData eventIcon(String type) {
    if (type.contains("FLAME") || type.contains("FIRE")) return Icons.local_fire_department;
    if (type.contains("VIBRATION")) return Icons.vibration;
    if (type.contains("UNLOCK")) return Icons.lock_open;
    if (type.contains("LOCK")) return Icons.lock;
    return Icons.notifications;
  }

  Color eventColor(String type) {
    if (type.contains("FLAME") || type.contains("FIRE")) return Colors.red;
    if (type.contains("VIBRATION")) return Colors.orange;
    if (type.contains("UNLOCK")) return Colors.green;
    return Colors.blueGrey;
  }

  String formatTime(String? time) {
    if (time == null || time.isEmpty) {
      return "";
    }

    try {
      final dt = DateTime.parse(time)
          .add(const Duration(hours: 7));

      return "${dt.day.toString().padLeft(2, '0')}/"
          "${dt.month.toString().padLeft(2, '0')}/"
          "${dt.year} "
          "${dt.hour.toString().padLeft(2, '0')}:"
          "${dt.minute.toString().padLeft(2, '0')}:"
          "${dt.second.toString().padLeft(2, '0')}";
    } catch (_) {
      return time;
    }
  }
}

class OtpDialog extends StatefulWidget {
  final int userId;
  final String otp;
  final VoidCallback onSuccess;

  const OtpDialog({
    super.key,
    required this.userId,
    required this.otp,
    required this.onSuccess,
  });

  @override
  State<OtpDialog> createState() => _OtpDialogState();
}

class _OtpDialogState extends State<OtpDialog> {
  final otpCtrl = TextEditingController();
  bool loading = false;
  String error = "";

  Future<void> verifyOtp() async {
    setState(() {
      loading = true;
      error = "";
    });

    final res = await http.post(
      Uri.parse("$baseUrl/api/verify-otp-open"),
      headers: {"Content-Type": "application/json"},
      body: jsonEncode({
        "user_id": widget.userId,
        "otp": otpCtrl.text.trim(),
      }),
    );

    final data = jsonDecode(res.body);

    if (data["success"] == true) {
      widget.onSuccess();
      if (!mounted) return;
      Navigator.pop(context);
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text("Đã gửi lệnh mở két đến ESP32")),
      );
    } else {
      setState(() => error = data["message"] ?? "OTP sai");
    }

    setState(() => loading = false);
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: const Text("Xác thực OTP"),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const Text("Mã OTP:"),
          const SizedBox(height: 8),
          Text(
            widget.otp,
            style: const TextStyle(fontSize: 30, color: Colors.teal, fontWeight: FontWeight.bold, letterSpacing: 4),
          ),
          const SizedBox(height: 16),
          TextField(
            controller: otpCtrl,
            keyboardType: TextInputType.number,
            decoration: const InputDecoration(labelText: "Nhập lại OTP", border: OutlineInputBorder()),
          ),
          if (error.isNotEmpty)
            Padding(
              padding: const EdgeInsets.only(top: 10),
              child: Text(error, style: const TextStyle(color: Colors.red)),
            ),
        ],
      ),
      actions: [
        TextButton(onPressed: () => Navigator.pop(context), child: const Text("Hủy")),
        FilledButton(
          onPressed: loading ? null : verifyOtp,
          child: loading ? const SizedBox(width: 18, height: 18, child: CircularProgressIndicator(strokeWidth: 2)) : const Text("Xác nhận"),
        ),
      ],
    );
  }
}