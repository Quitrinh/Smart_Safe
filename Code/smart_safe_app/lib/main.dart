import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'dart:async';
import 'package:url_launcher/url_launcher.dart';
import 'package:geocoding/geocoding.dart';
import 'service/backend_service.dart';
//import 'pages/login_page.dart';
import 'package:flutter_svg/flutter_svg.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_messaging/firebase_messaging.dart';
import 'package:flutter/foundation.dart';
import 'firebase_options.dart'; // <-- file được sinh bởi FlutterFire CLI

const String baseUrl = "https://smart-safe-api-etd9a7bsbhb6gyh8.southeastasia-01.azurewebsites.net";

// void main() {
//   runApp(const SmartSafeApp());
// }

// class SmartSafeApp extends StatelessWidget {
//   const SmartSafeApp({super.key});

//   @override
//   Widget build(BuildContext context) {
//     return MaterialApp(
//       title: "Smart Safe",
//       debugShowCheckedModeBanner: false,
//       theme: ThemeData(
//         colorSchemeSeed: Colors.teal,
//         useMaterial3: true,
//         fontFamily: "Arial",
//       ),
//       home: const LoginPage(),
//     );
//   }
// }
@pragma('vm:entry-point')
Future<void> firebaseMessagingBackgroundHandler(RemoteMessage message) async {
  await Firebase.initializeApp(
    options: DefaultFirebaseOptions.currentPlatform,
  );

  print("Background message: ${message.notification?.title}");
}

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  try {
    await Firebase.initializeApp(
      options: DefaultFirebaseOptions.currentPlatform,
    );

    FirebaseMessaging.onBackgroundMessage(firebaseMessagingBackgroundHandler);

    FirebaseMessaging.onMessage.listen((RemoteMessage message) {
      print(
        'Foreground message: ${message.notification?.title} - ${message.notification?.body}',
      );
    });

    FirebaseMessaging.onMessageOpenedApp.listen((RemoteMessage message) {
      print('Message clicked!');
    });
  } catch (e) {
    print("FIREBASE INIT ERROR: $e");
  }

  try {
    await BackendService.testBackend();
  } catch (e) {
    print("BACKEND TEST ERROR: $e");
  }

  runApp(const SmartSafeApp());
}


class SmartSafeApp extends StatelessWidget {
  const SmartSafeApp({super.key});

@override
Widget build(BuildContext context) {
  return MaterialApp(
    title: "SMART SAFE",
    debugShowCheckedModeBanner: false,
    theme: ThemeData(
      colorSchemeSeed: const Color.fromARGB(255, 222, 238, 236),
      useMaterial3: true,
      fontFamily: "Arial",
      textTheme: TextTheme(
        displayLarge: TextStyle(
          fontSize: 32,
          fontWeight: FontWeight.bold,
          color: Colors.teal, // màu chủ đạo cho header
        ),
        displayMedium: TextStyle(
          fontSize: 40,
          fontWeight: FontWeight.bold,
          color: Colors.tealAccent,
        ),
        headlineSmall: TextStyle(
          fontSize: 24,
          fontWeight: FontWeight.w600,
          color: Colors.teal,
        ),
        titleLarge: TextStyle(
          fontSize: 20,
          fontWeight: FontWeight.w600,
          color: Colors.teal.shade700,
        ),
        bodyMedium: TextStyle(
          fontSize: 16,
          color: Colors.black87,
        ),
      ),
      elevatedButtonTheme: ElevatedButtonThemeData(
        style: ElevatedButton.styleFrom(
          textStyle: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
          padding: const EdgeInsets.symmetric(vertical: 16, horizontal: 24),
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(16),
          ),
        ),
      ),
    ),
    home: const LoginPage(),
  );
}
}
class RegisterPage extends StatefulWidget {
  const RegisterPage({super.key});

  @override
  State<RegisterPage> createState() => _RegisterPageState();
}
class _RegisterPageState extends State<RegisterPage> {
  final fullNameController = TextEditingController();
  final usernameController = TextEditingController();
  final phoneController = TextEditingController();
  final passwordController = TextEditingController();
  bool loading = false;

Future<void> registerAccount() async {
  if (loading) return;
  final fullName = fullNameController.text.trim();
  final username = usernameController.text.trim().toLowerCase();
  final phone = phoneController.text.trim();
  final password = passwordController.text.trim();


  if (fullName.isEmpty || phone.isEmpty || password.isEmpty || username.isEmpty) {
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(
        content: Text("Vui lòng nhập đầy đủ thông tin"),
      ),
    );
    return;
  }

  if (password.length < 6) {
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(
        content: Text("Mật khẩu phải từ 6 ký tự"),
      ),
    );
    return;
  }

  setState(() {
    loading = true;
  });

  try {
    final res = await http.post(
      Uri.parse("$baseUrl/api/auth/register"),
      headers: {
        "Content-Type": "application/json",
      },
      body: jsonEncode({
        "full_name": fullName,
        "username": username,
        "phone": phone,
        "password": password,
      }),
    );
    print("REGISTER SEND:");
    print(jsonEncode({
      "full_name": fullName,
      "username": username,
      "phone": phone,
      "password": password,
    }));
    print("REGISTER STATUS: ${res.statusCode}");
    print("REGISTER BODY: ${res.body}");

    final data = jsonDecode(res.body);

    if ((res.statusCode == 200 || res.statusCode == 201) &&
        data["success"] == true) {
      if (!mounted) return;

      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text("Đăng ký thành công, vui lòng chờ admin phê duyệt"),
        ),
      );

      Navigator.pop(context);
      return;
    }

    if (!mounted) return;

    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(
          data["message"] ??
              data["error"] ??
              "Đăng ký không thành công",
        ),
      ),
    );
  } catch (e) {
    print("REGISTER ERROR: $e");

    if (!mounted) return;

    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text("Lỗi đăng ký: $e"),
      ),
    );
  } finally {
    if (mounted) {
      setState(() {
        loading = false;
      });
    }
  }
}

  @override
  void dispose() {
    fullNameController.dispose();
    usernameController.dispose();
    phoneController.dispose();
    passwordController.dispose();
    super.dispose();
  }
  @override
Widget build(BuildContext context) {
  return Scaffold(
    backgroundColor: const Color.fromARGB(255, 119, 108, 186),
    appBar: AppBar(
      title: const Text("Đăng ký tài khoản"),
      centerTitle: true,
    ),
    body: Center(
      child: SingleChildScrollView(
        padding: const EdgeInsets.all(24),
        child: Card(
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(20),
          ),
          child: Padding(
            padding: const EdgeInsets.all(20),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                const Icon(
                  Icons.person_add_alt_1,
                  size: 64,
                  color: Colors.green,
                ),

                const SizedBox(height: 16),

                const Text(
                  "Đăng ký tài khoản",
                  style: TextStyle(
                    fontSize: 24,
                    fontWeight: FontWeight.bold,
                  ),
                ),

                const SizedBox(height: 8),

              TextField(
                controller: fullNameController,
                decoration: const InputDecoration(
                  labelText: "Họ và tên",
                  prefixIcon: Icon(Icons.badge),
                  border: OutlineInputBorder(),
                ),
              ),

              const SizedBox(height: 16),

              TextField(
                controller: usernameController,
                decoration: const InputDecoration(
                  labelText: "Tên đăng nhập",
                  prefixIcon: Icon(Icons.account_circle),
                  border: OutlineInputBorder(),
                  helperText: "Ví dụ: Cường, Tuấn, Hcmute",
                ),
              ),

              const SizedBox(height: 16),

              TextField(
                controller: phoneController,
                keyboardType: TextInputType.phone,
                decoration: const InputDecoration(
                  labelText: "Số điện thoại",
                  prefixIcon: Icon(Icons.phone),
                  border: OutlineInputBorder(),
                  helperText: "            ",
                ),
              ),

                TextField(
                  controller: passwordController,
                  obscureText: true,
                  decoration: const InputDecoration(                    
                    labelText: "Mật khẩu",
                    prefixIcon: Icon(Icons.lock),
                    border: OutlineInputBorder(),
                  ),
                ),

                const SizedBox(height: 23),

                SizedBox(
                  width: double.infinity,
                  height: 50,
                  child: ElevatedButton(
                    onPressed: loading ? null : registerAccount,
                    child: loading
                        ? const CircularProgressIndicator(
                            color: Color.fromARGB(255, 187, 226, 133),
                          )
                        : const Text("Đăng ký"),
                  ),
                ),

                const SizedBox(height: 12),

                TextButton(
                  onPressed: () {
                    Navigator.pop(context);
                  },
                  child: const Text("Đã có tài khoản? Đăng nhập"),
                ),
              ],
            ),
          ),
        ),
      ),
    ),
  );
}
}
class LoginPage extends StatefulWidget {
  const LoginPage({super.key});

  @override
  State<LoginPage> createState() => _LoginPageState();
}

class _LoginPageState extends State<LoginPage> {
  final loginController = TextEditingController();
  final passwordController = TextEditingController();

  bool loading = false;

  Future<void> login() async {
    if (loading) return;

    setState(() {
      loading = true;
    });

    try {
      final res = await http.post(
        Uri.parse("$baseUrl/api/auth/login"),
        headers: {
          "Content-Type": "application/json",
        },
        body: jsonEncode({
          "login": loginController.text.trim(),
          "password": passwordController.text.trim(),
        }),
      );

      final data = jsonDecode(res.body);

      if (res.statusCode == 200 && data["success"] == true) {
        final token = data["token"];
        final user = data["user"];

        final int userId = user["id"];
        final String role = user["role"] ?? "user";

        if (!mounted) return;

        Navigator.pushReplacement(
          context,
          MaterialPageRoute(
            builder: (_) => DashboardPage(
              token: token,
              userId: userId,
              username: user["username"] ?? "User",
              role: role,
            ),
          ),
        );

        return;
      }

      if (res.statusCode == 403 && data["code"] == "ACCOUNT_PENDING") {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text("Tài khoản đang chờ admin phê duyệt"),
          ),
        );
        return;
      }

      if (res.statusCode == 403 && data["code"] == "ACCOUNT_REJECTED") {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text("Tài khoản đã bị admin từ chối"),
          ),
        );
        return;
      }

      if (res.statusCode == 423) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(data["message"] ?? "Tài khoản đang bị khóa"),
          ),
        );
        return;
      }

      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(data["message"] ?? "Đăng nhập thất bại"),
        ),
      );
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text("Không kết nối được server"),
        ),
      );
    } finally {
      if (mounted) {
        setState(() {
          loading = false;
        });
      }
    }
  }

  @override
  void dispose() {
    loginController.dispose();
    passwordController.dispose();
    super.dispose();
  }

@override
Widget build(BuildContext context) {
  return Scaffold(
    backgroundColor: const Color.fromARGB(255, 26, 88, 134),
    appBar: AppBar(
      title: const Text("Đăng nhập"),
      centerTitle: true,
    ),
    body: Center(
      child: SingleChildScrollView(
        padding: const EdgeInsets.all(24),
        child: Card(
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(20),
          ),
          child: Padding(
            padding: const EdgeInsets.all(20),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                const Icon(
                  Icons.lock_outline,
                  size: 64,
                  color: Color.fromARGB(255, 248, 252, 255),
                ),

                const SizedBox(height: 16),

                const Text(
                  "SMART SAFE",
                  style: TextStyle(
                    fontSize: 24,
                    fontWeight: FontWeight.bold,
                  ),
                ),

                const SizedBox(height: 24),

                TextField(
                  controller: loginController,
                  decoration: const InputDecoration(
                    labelText: "Số điện thoại / Username",
                    prefixIcon: Icon(Icons.person),
                    border: OutlineInputBorder(),
                  ),
                ),

                const SizedBox(height: 16),

                TextField(
                  controller: passwordController,
                  obscureText: true,
                  decoration: const InputDecoration(
                    labelText: "Mật khẩu",
                    prefixIcon: Icon(Icons.lock),
                    border: OutlineInputBorder(),
                  ),
                ),

                const SizedBox(height: 24),

                SizedBox(
                  width: double.infinity,
                  height: 52,
                  child: ElevatedButton(
                    onPressed: loading ? null : login,
                    child: loading
                        ? const CircularProgressIndicator(
                            color: Color.fromARGB(255, 1, 1, 0),
                          )
                        : const Text("Đăng nhập"),
                  ),
                ),

                Align(
                  alignment: Alignment.centerRight,
                  child: TextButton(
                    onPressed: () {
                      Navigator.push(
                        context,
                        MaterialPageRoute(
                          builder: (_) => const ForgotPasswordPage(),
                        ),
                      );
                    },
                    child: const Text("Quên mật khẩu?"),
                  ),
                ),
                const SizedBox(height: 16),

                TextButton(
                  onPressed: () {
                    Navigator.push(
                      context,
                      MaterialPageRoute(
                        builder: (_) => RegisterPage(),
                      ),
                    );
                  },
                  child: const Text("Chưa có tài khoản? Đăng ký"),
                ),
              ],
            ),
          ),
        ),
      ),
    ),
  );
}
}
class ForgotPasswordPage extends StatefulWidget {
  const ForgotPasswordPage({super.key});

  @override
  State<ForgotPasswordPage> createState() => _ForgotPasswordPageState();
}

class _ForgotPasswordPageState extends State<ForgotPasswordPage> {
  final loginController = TextEditingController();
  final otpController = TextEditingController();
  final newPasswordController = TextEditingController();
  final confirmPasswordController = TextEditingController();

  bool loading = false;
  bool otpSent = false;

  @override
  void dispose() {
    loginController.dispose();
    otpController.dispose();
    newPasswordController.dispose();
    confirmPasswordController.dispose();
    super.dispose();
  }

  Future<void> requestOtp() async {
    final login = loginController.text.trim();

    if (login.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text("Vui lòng nhập username hoặc số điện thoại")),
      );
      return;
    }

    setState(() {
      loading = true;
    });

    try {
      final res = await http.post(
        Uri.parse("$baseUrl/api/auth/forgot-password/request"),
        headers: {
          "Content-Type": "application/json",
        },
        body: jsonEncode({
          "login": login,
        }),
      );

      final data = jsonDecode(res.body);

      if (!mounted) return;

      if (res.statusCode == 200 && data["success"] == true) {
        setState(() {
          otpSent = true;
        });

        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(data["message"] ?? "Đã gửi OTP"),
          ),
        );
      } else {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(data["message"] ?? "Không gửi được OTP"),
          ),
        );
      }
    } catch (e) {
      if (!mounted) return;

      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text("Lỗi gửi OTP: $e")),
      );
    } finally {
      if (mounted) {
        setState(() {
          loading = false;
        });
      }
    }
  }

  Future<void> resetPassword() async {
    final login = loginController.text.trim();
    final otp = otpController.text.trim();
    final newPassword = newPasswordController.text.trim();
    final confirmPassword = confirmPasswordController.text.trim();

    if (login.isEmpty || otp.isEmpty || newPassword.isEmpty || confirmPassword.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text("Vui lòng nhập đầy đủ thông tin")),
      );
      return;
    }

    if (newPassword.length < 6) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text("Mật khẩu mới phải từ 6 ký tự")),
      );
      return;
    }

    if (newPassword != confirmPassword) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text("Mật khẩu xác nhận không khớp")),
      );
      return;
    }

    setState(() {
      loading = true;
    });

    try {
      final res = await http.post(
        Uri.parse("$baseUrl/api/auth/forgot-password/reset"),
        headers: {
          "Content-Type": "application/json",
        },
        body: jsonEncode({
          "login": login,
          "otp": otp,
          "new_password": newPassword,
        }),
      );

      final data = jsonDecode(res.body);

      if (!mounted) return;

      if (res.statusCode == 200 && data["success"] == true) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(data["message"] ?? "Đổi mật khẩu thành công"),
          ),
        );

        Navigator.pop(context);
      } else {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(data["message"] ?? "Đổi mật khẩu thất bại"),
          ),
        );
      }
    } catch (e) {
      if (!mounted) return;

      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text("Lỗi đổi mật khẩu: $e")),
      );
    } finally {
      if (mounted) {
        setState(() {
          loading = false;
        });
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFFF3FAF9),
      appBar: AppBar(
        title: const Text("Quên mật khẩu"),
        backgroundColor: const Color(0xFFE0F2F1),
        foregroundColor: const Color.fromARGB(255, 247, 67, 184),
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(20),
        child: Card(
          elevation: 2,
          child: Padding(
            padding: const EdgeInsets.all(20),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                const Icon(
                  Icons.lock_reset,
                  size: 64,
                  color: Colors.teal,
                ),

                const SizedBox(height: 16),

                const Text(
                  "Đặt lại mật khẩu",
                  textAlign: TextAlign.center,
                  style: TextStyle(
                    fontSize: 22,
                    fontWeight: FontWeight.bold,
                  ),
                ),

                const SizedBox(height: 8),

                const Text(
                  "Nhập tên đăng nhập hoặc số điện thoại để nhận mã OTP.",
                  textAlign: TextAlign.center,
                ),

                const SizedBox(height: 24),

                TextField(
                  controller: loginController,
                  enabled: !otpSent,
                  decoration: const InputDecoration(
                    labelText: "Tên đăng nhập hoặc số điện thoại",
                    prefixIcon: Icon(Icons.person),
                    border: OutlineInputBorder(),
                  ),
                ),

                const SizedBox(height: 16),

                if (!otpSent)
                  FilledButton.icon(
                    onPressed: loading ? null : requestOtp,
                    icon: const Icon(Icons.sms),
                    label: loading
                        ? const Text("Đang gửi...")
                        : const Text("Gửi mã OTP"),
                  ),

                if (otpSent) ...[
                  TextField(
                    controller: otpController,
                    keyboardType: TextInputType.number,
                    decoration: const InputDecoration(
                      labelText: "Mã OTP",
                      prefixIcon: Icon(Icons.password),
                      border: OutlineInputBorder(),
                    ),
                  ),

                  const SizedBox(height: 16),

                  TextField(
                    controller: newPasswordController,
                    obscureText: true,
                    decoration: const InputDecoration(
                      labelText: "Mật khẩu mới",
                      prefixIcon: Icon(Icons.lock),
                      border: OutlineInputBorder(),
                    ),
                  ),

                  const SizedBox(height: 16),

                  TextField(
                    controller: confirmPasswordController,
                    obscureText: true,
                    decoration: const InputDecoration(
                      labelText: "Nhập lại mật khẩu mới",
                      prefixIcon: Icon(Icons.lock_outline),
                      border: OutlineInputBorder(),
                    ),
                  ),

                  const SizedBox(height: 20),

                  FilledButton.icon(
                    onPressed: loading ? null : resetPassword,
                    icon: const Icon(Icons.check_circle),
                    label: loading
                        ? const Text("Đang xử lý...")
                        : const Text("Xác nhận đổi mật khẩu"),
                  ),

                  const SizedBox(height: 8),

                  TextButton(
                    onPressed: loading ? null : requestOtp,
                    child: const Text("Gửi lại mã OTP"),
                  ),
                ],
              ],
            ),
          ),
        ),
      ),
    );
  }
}
class AppTabItem {
  final String title;
  final IconData icon;
  final Widget Function() builder;

  AppTabItem({
    required this.title,
    required this.icon,
    required this.builder,
  });
}
class DashboardPage extends StatefulWidget {
  final int userId;
  final String username;
  final String role;
  final String token;

  const DashboardPage({
    super.key,
    required this.userId,
    required this.username,
    required this.role,
    required this.token,
  });

  @override
  State<DashboardPage> createState() => _DashboardPageState();
}
class _DashboardPageState extends State<DashboardPage> {
  int tabIndex = 0;
  List authMethods = [];
  Map<String, dynamic>? safeStatus;
  Map<String, dynamic>? locationConfig;
  Map<String, dynamic>? currentGps;
  List pendingUsers = [];
  bool pendingUsersLoading = false;
  List users = [];
  bool usersLoading = false;
  List events = [];
  List smsReceivers = [];
  List configs = [];
  bool loading = false;
  bool editingMode = false; // bật/tắt chế độ xoá
  bool get isAdmin => widget.role == "admin";
  Map<int, bool> selectedEvents = {}; // id sự kiện -> true/false
  bool showDeletedEvents = false;
  Timer? statusTimer;
  Timer? notificationTimer;
  String? currentDeviceToken;
  late final FirebaseMessaging _fcm;
  final TextEditingController phoneController = TextEditingController();
  final usernameController = TextEditingController();
  final TextEditingController otpController = TextEditingController();
  final TextEditingController newPasswordController = TextEditingController();
  List notifications = [];
  int unreadCount = 0;
  bool pushEnabled = true;
  bool pushLoading = false;
  Map<String, String> get authHeaders => {
  "Content-Type": "application/json",
  "Authorization": "Bearer ${widget.token}",
  };
  double? toDoubleValue(dynamic value) {
  if (value == null) return null;
  if (value is double) return value;
  if (value is int) return value.toDouble();
  return double.tryParse(value.toString());
}

Future<String> getAddressFromLatLng(double lat, double lng) async {
  try {
    final placemarks = await placemarkFromCoordinates(lat, lng);

    if (placemarks.isEmpty) {
      return "Không xác định được địa chỉ";
    }

    final p = placemarks.first;

    final parts = [
      p.street,
      p.subLocality,
      p.locality,
      p.administrativeArea,
      p.country,
    ].where((e) => e != null && e.trim().isNotEmpty).toList();

    if (parts.isEmpty) {
      return "Không xác định được địa chỉ";
    }

    return parts.join(", ");
  } catch (e) {
    print("getAddressFromLatLng error: $e");
    return "Không lấy được địa chỉ từ GPS";
  }
}
  List<AppTabItem> get visibleTabs {
  final tabs = <AppTabItem>[
    AppTabItem(
      title: "Trang chủ",
      icon: Icons.dashboard,
      builder: () => dashboardView(),
    ),

    AppTabItem(
      title: "Thông báo",
      icon: Icons.notifications,
      builder: () => notificationsView(),
    ),

    AppTabItem(
      title: "Cài đặt",
      icon: Icons.settings,
      builder: () => settingsView(),
    ),
  ];

  if (isAdmin) {
    tabs.insertAll(1, [
      AppTabItem(
        title: "SMS",
        icon: Icons.sms,
        builder: () => smsView(),
      ),

      AppTabItem(
        title: "Xác thực",
        icon: Icons.fingerprint,
        builder: () => authMethodsView(),
      ),
    ]);
  }

  return tabs;
}
  @override
void initState() {
  super.initState();

  // 1. Khởi tạo FCM
  initFCM();

  // 2. Load dữ liệu lần đầu
  refreshAll();

  if (isAdmin) {
    fetchPendingUsers();
    fetchSmsReceivers();
    fetchUsers();
  }

  // 3. Timer cập nhật trạng thái két
  statusTimer = Timer.periodic(
    const Duration(seconds: 2),
    (_) {
      if (!mounted) return;
      fetchSafeStatus();
    },
  );

  // 4. Timer cập nhật thông báo + pending user
  notificationTimer = Timer.periodic(
    const Duration(seconds: 5),
    (_) {
      if (!mounted) return;

      fetchNotifications();

      if (isAdmin) {
        fetchPendingUsers();
      }
    },
  );
}
Future<void> initFCM() async {
  try {
    if (kIsWeb) {
      print("[FCM] Skip FCM on Web");
      return;
    }

    _fcm = FirebaseMessaging.instance;

    final settings = await _fcm.requestPermission(
      alert: true,
      badge: true,
      sound: true,
    );

    print("[FCM] Permission: ${settings.authorizationStatus}");

    final token = await _fcm.getToken();

    print("[FCM TOKEN] $token");

    if (token != null) {
      currentDeviceToken = token;
      await registerDeviceToken(token);
    }

    _fcm.onTokenRefresh.listen((newToken) async {
      print("[FCM TOKEN REFRESH] $newToken");

      currentDeviceToken = newToken;
      await registerDeviceToken(newToken);
    });

    FirebaseMessaging.onMessage.listen((RemoteMessage message) async {
      print("[FCM FOREGROUND] ${message.messageId}");
      print("[FCM TITLE] ${message.notification?.title}");
      print("[FCM BODY] ${message.notification?.body}");
      print("[FCM DATA] ${message.data}");

      await fetchNotifications();

      if (!mounted) return;

      final title =
          message.notification?.title ??
          message.data["title"] ??
          "Smart Safe";

      final body =
          message.notification?.body ??
          message.data["body"] ??
          "Có thông báo mới";

      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text("$title\n$body"),
          duration: const Duration(seconds: 4),
        ),
      );
    });
  } catch (e) {
    print("[FCM INIT ERROR] $e");
  }
}
Future<void> registerDeviceToken(String token) async {
  try {
    final res = await http.post(
      Uri.parse("$baseUrl/api/device-tokens"),
      headers: authHeaders,
      body: jsonEncode({
        "device_token": token,
        "platform": "android",
      }),
    );

    print("[REGISTER TOKEN STATUS] ${res.statusCode}");
    print("[REGISTER TOKEN BODY] ${res.body}");
  } catch (e) {
    print("[REGISTER TOKEN ERROR] $e");
  }
}
  @override
  void dispose() {
    statusTimer?.cancel();
    notificationTimer?.cancel();
    phoneController.dispose();
    usernameController.dispose();
    otpController.dispose();
    newPasswordController.dispose();
    super.dispose();
  }
Future<void> refreshAll() async {
  await fetchSafeStatus();
  await fetchNotifications();

  if (isAdmin) {
    await fetchLocationConfig();
    await fetchEvents();
    await fetchUsers();
    await fetchPendingUsers();
    await fetchSmsReceivers();
    await fetchConfig();
    await fetchAuthMethods();
  }
}

Future<void> fetchSafeStatus() async {
  try {
    final res = await http.get(
      Uri.parse("$baseUrl/api/safe/status"),
      headers: authHeaders,
    );

    print("SAFE STATUS: ${res.statusCode}");
    print("SAFE BODY: ${res.body}");

    if (res.statusCode != 200) return;

    final data = jsonDecode(res.body);

    if (!mounted) return;

    if (data["success"] == true) {
      final status = data["data"];

      setState(() {
        safeStatus = status;

        if (status["gps_lat"] != null &&
            status["gps_lng"] != null &&
            status["gps_lat"].toString() != "0" &&
            status["gps_lng"].toString() != "0") {
          currentGps = {
            "gps_lat": status["gps_lat"],
            "gps_lng": status["gps_lng"],
          };
        }
      });
    }
  } catch (e) {
    print("fetchSafeStatus error: $e");
  }
}

  // Future<void> fetchEvents() async {
  //   setState(() => loading = true);
  //   try {
  //     final res = await http.get(Uri.parse("$baseUrl/api/events"));
  //     final data = jsonDecode(res.body);
  //     if (data["success"] == true) {
  //       setState(() => events = data["data"]);
  //     }
  //   } catch (_) {}
  //   setState(() => loading = false);
  // }
Future<void> fetchEvents() async {
  try {
    final res = await http.get(Uri.parse("$baseUrl/api/events"));

    final data = jsonDecode(res.body);

    if (res.statusCode == 200 && data["success"] == true) {
      setState(() {
        events = List<Map<String, dynamic>>.from(data["data"] ?? []);
      });
    } else {
      setState(() {
        events = []; // tránh giữ dữ liệu cũ
      });
    }
  } catch (e) {
    print("fetchEvents error: $e");
    setState(() {
      events = [];
    });
  }
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
    final res = await http.get(
      Uri.parse("$baseUrl/api/admin/config"),
      headers: authHeaders,
    );

    print("CONFIG STATUS: ${res.statusCode}");
    print("CONFIG BODY: ${res.body}");

    final data = jsonDecode(res.body);

    if (!mounted) return;

    if (res.statusCode == 200 && data["success"] == true) {
      setState(() {
        configs = data["data"] ?? [];
      });
    } else {
      print("fetchConfig failed: ${data["message"] ?? data["error"]}");
    }
  } catch (e) {
    print("fetchConfig error: $e");
  }
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
    headers: authHeaders,
    body: jsonEncode({}),
  );

  final data = jsonDecode(res.body);

  if (data["success"] == true && mounted) {
    showDialog(
      context: context,
      builder: (_) => OtpDialog(
        userId: widget.userId,
        otp: data["debug_otp"]?.toString() ?? data["otp"]?.toString() ?? "",
        token: widget.token,
        onSuccess: refreshAll,
      ),
    );
  } else {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(data["message"] ?? "Không tạo được OTP")),
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
            decoration: const InputDecoration(
              labelText: "Mật khẩu cũ",
            ),
          ),
          const SizedBox(height: 12),
          TextField(
            controller: newCtrl,
            obscureText: true,
            decoration: const InputDecoration(
              labelText: "Mật khẩu mới",
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
            try {
              final res = await http.post(
                Uri.parse("$baseUrl/api/auth/change-password"),
                headers: authHeaders,
                body: jsonEncode({
                  "old_password": oldCtrl.text.trim(),
                  "new_password": newCtrl.text.trim(),
                }),
              );

              print("CHANGE PASS STATUS: ${res.statusCode}");
              print("CHANGE PASS BODY: ${res.body}");

              final data = jsonDecode(res.body);

              if (!mounted) return;
              Navigator.pop(context);

              ScaffoldMessenger.of(context).showSnackBar(
                SnackBar(
                  content: Text(data["message"] ?? "Đã xử lý"),
                ),
              );
            } catch (e) {
              print("CHANGE PASS ERROR: $e");

              if (!mounted) return;
              Navigator.pop(context);

              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(
                  content: Text("Không kết nối được backend"),
                ),
              );
            }
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
  Future<void> requestForgotPasswordOtp(String login) async {
    final res = await http.post(
      Uri.parse("$baseUrl/api/auth/forgot-password/request"),
      headers: {
        "Content-Type": "application/json",
      },
      body: jsonEncode({
        "login": login,
      }),
    );

    final data = jsonDecode(res.body);
    print(data);
  }
  Future<void> resetPassword({
    required String login,
    required String otp,
    required String newPassword,
  }) async {
    final res = await http.post(
      Uri.parse("$baseUrl/api/auth/forgot-password/reset"),
      headers: {
        "Content-Type": "application/json",
      },
      body: jsonEncode({
        "login": login,
        "otp": otp,
        "new_password": newPassword,
      }),
    );

    final data = jsonDecode(res.body);
    print(data);
  }
  Future<void> changePassword({
  required String oldPassword,
  required String newPassword,
}) async {
  final res = await http.patch(
    Uri.parse("$baseUrl/api/auth/change-password"),
    headers: authHeaders,
    body: jsonEncode({
      "old_password": oldPassword,
      "new_password": newPassword,
    }),
  );

  final data = jsonDecode(res.body);
  print(data);
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
    setState(() => loading = true);
    try {
      final res = await http.get(Uri.parse("$baseUrl/api/auth-methods"));
      final data = json.decode(res.body);
      if (data["success"] == true) {
        setState(() {
          authMethods = data["data"];
        });
      }
    } catch (e) {
      print("Error fetching auth methods: $e");
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text("Lỗi khi tải danh sách xác thực")),
      );
    } finally {
      setState(() => loading = false);
    }
  }
  // Xóa auth method
  Future<void> removeAuthMethod(String methodType, String methodValue) async {
    try {
      final res = await http.post(
        Uri.parse("$baseUrl/api/auth-methods/remove"),
        headers: {"Content-Type": "application/json"},
        body: json.encode({
          "method_type": methodType,
          "method_value": methodValue,
        }),
      );
      final data = json.decode(res.body);
      if (data["success"] == true) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text("${methodType} đã xóa thành công")),
        );
        await fetchAuthMethods();
      } else {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text("Xóa thất bại: ${data["message"]}")),
        );
      }
    } catch (e) {
      print("Error removing auth method: $e");
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text("Lỗi khi xóa auth method")),
      );
    }
  }
 // Thêm auth method (RFID hoặc Fingerprint)
  Future<void> enrollAuthMethod(String methodType) async {
    final adminCtrl = TextEditingController();
    final nameCtrl = TextEditingController();

    showDialog(
      context: context,
      builder: (_) => AlertDialog(
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
        title: Text(methodType == "RFID" ? "Thêm thẻ RFID" : "Thêm vân tay"),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: adminCtrl,
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
          TextButton(onPressed: () => Navigator.pop(context), child: const Text("Hủy")),
          ElevatedButton(
            onPressed: () async {
              // // Verify admin password
              // final verifyRes = await http.post(
              //   Uri.parse("$baseUrl/api/admin/verify-password"),
              //   headers: {"Content-Type": "application/json"},
              //   body: jsonEncode({"password": adminCtrl.text.trim()}),
              // );
              // final verifyData = jsonDecode(verifyRes.body);
              // if (verifyData["success"] != true) {
              //   if (!mounted) return;
              //   ScaffoldMessenger.of(context).showSnackBar(
              //     const SnackBar(content: Text("Sai mật khẩu admin")),
              //   );
              //   return;
              // }

              // Gửi lệnh enroll đến backend
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

Future<void> removeEvents(List<int> ids) async {
  try {
    final res = await http.post(
      Uri.parse("$baseUrl/api/events/remove"),
      headers: {"Content-Type": "application/json"},
      body: jsonEncode({"ids": ids}),
    );

    print("STATUS: ${res.statusCode}");
    print("BODY: ${res.body}");

    final data = jsonDecode(res.body);

    if (res.statusCode == 200 && data["success"] == true) {

      setState(() {
        selectedEvents.clear();
        editingMode = false;
      });

      // 🔥 2. sync lại DB (source of truth)
      await fetchEvents();

      // 🔥 3. show success
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text("Xoá thành công")),
      );

    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text("Lỗi xoá: ${data["message"] ?? res.body}"),
        ),
      );
    }

  } catch (e) {
    print("REMOVE ERROR: $e");

    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text("Lỗi khi xoá sự kiện")),
    );
  }
}

Future<void> restoreEvents(List<int> ids) async {
  if (ids.isEmpty) {
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text("Chưa chọn sự kiện nào để khôi phục")),
    );
    return;
  }

  try {
    final res = await http.post(
      Uri.parse("$baseUrl/api/events/restore"),
      headers: {"Content-Type": "application/json"},
      body: jsonEncode({"ids": ids.map((e) => e.toInt()).toList()}),
    );

    final data = jsonDecode(res.body);

    if (res.statusCode == 200 && data["success"] == true) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text("Khôi phục thành công")),
      );

      await fetchEvents(); // đồng bộ lại danh sách từ DB
      setState(() {
        editingMode = false;
        selectedEvents.clear();
      });
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text("Restore thất bại: ${data["message"] ?? res.body}")),
      );
    }
  } catch (e) {
    print("RESTORE ERROR: $e");
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text("Lỗi khi khôi phục sự kiện")),
    );
  }
}
Future<void> fetchDeletedEvents() async {
  try {
    final res = await http.get(Uri.parse("$baseUrl/api/events?status=deleted"));
    final data = jsonDecode(res.body);

    if (data["success"] == true) {
      setState(() {
        // Ép kiểu rõ ràng sang List<Map<String, dynamic>>
        events = List<Map<String, dynamic>>.from(data["data"]);
      });
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text("Lỗi khi tải sự kiện: ${data["message"] ?? "Unknown"}")),
      );
    }
  } catch (e) {
    print("Error fetching deleted events: $e");
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text("Lỗi khi tải danh sách sự kiện đã xoá")),
    );
  }
}
Future<void> requestOTP() async {
  final phone = phoneController.text.trim();
  if (phone.isEmpty) return;

  final res = await http.post(
    Uri.parse("$baseUrl/api/auth/request-reset-password"),
    headers: {"Content-Type": "application/json"},
    body: jsonEncode({
      "login": phone,
    }),
  );

  final data = jsonDecode(res.body);

  ScaffoldMessenger.of(context).showSnackBar(
    SnackBar(content: Text(data["message"] ?? "OTP requested")),
  );
}

 Future<void> verifyOTP() async {
  final phone = phoneController.text.trim();
  final otp = otpController.text.trim();
  final newPassword = newPasswordController.text.trim();

  if (phone.isEmpty || otp.isEmpty || newPassword.isEmpty) return;

  final res = await http.post(
    Uri.parse("$baseUrl/api/auth/reset-password"),
    headers: {"Content-Type": "application/json"},
    body: jsonEncode({
      "login": phone,
      "otp": otp,
      "new_password": newPassword,
    }),
  );

  final data = jsonDecode(res.body);

  ScaffoldMessenger.of(context).showSnackBar(
    SnackBar(content: Text(data["message"] ?? "OTP verified")),
  );
}
// Future<void> registerDeviceToken(String token) async {
//   try {
//     final res = await http.post(
//       Uri.parse("$baseUrl/api/device-tokens"),
//       headers: authHeaders,
//       body: jsonEncode({
//         "device_token": token,
//         "platform": "android",
//       }),
//     );

//     print("Register device token: ${res.body}");
//   } catch (e) {
//     print("registerDeviceToken error: $e");
//   }
// }
Future<void> fetchPushStatus() async {
  try {
    final res = await http.get(
      Uri.parse("$baseUrl/api/device-tokens/status"),
      headers: authHeaders,
    );

    print("PUSH STATUS CODE: ${res.statusCode}");
    print("PUSH STATUS BODY: ${res.body}");

    final data = jsonDecode(res.body);

    if (res.statusCode == 200 && data["success"] == true) {
      setState(() {
        pushEnabled = data["enabled"] == true;
      });
    }
  } catch (e) {
    print("fetchPushStatus error: $e");
  }
}
Future<void> fetchNotifications() async {
  try {
    final res = await http.get(
      Uri.parse("$baseUrl/api/notifications"),
      headers: authHeaders,
    );

    final data = jsonDecode(res.body);

    if (res.statusCode == 200 && data["success"] == true) {
      final list = List<Map<String, dynamic>>.from(data["data"] ?? []);

      setState(() {
        notifications = list;
        unreadCount = list.where((n) => n["is_read"] == 0).length;
      });
    }
  } catch (e) {
    print("fetchNotifications error: $e");
  }
}
Future<void> markNotificationRead(int id) async {
  try {
    await http.patch(
      Uri.parse("$baseUrl/api/notifications/$id/read"),
      headers: authHeaders,
    );

    await fetchNotifications();
  } catch (e) {
    print("markNotificationRead error: $e");
  }
}
Future<void> markAllNotificationsRead() async {
  try {
    final res = await http.patch(
      Uri.parse("$baseUrl/api/notifications/read-all"),
      headers: authHeaders,
    );

    final data = jsonDecode(res.body);

    if (res.statusCode == 200 && data["success"] == true) {
      await fetchNotifications();

      if (!mounted) return;

      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text("Đã đánh dấu tất cả là đã đọc")),
      );
    } else {
      if (!mounted) return;

      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(data["message"] ?? "Không xử lý được")),
      );
    }
  } catch (e) {
    print("markAllNotificationsRead error: $e");

    if (!mounted) return;

    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text("Lỗi khi đánh dấu đã đọc")),
    );
  }
}
// Future<void> turnOffAlarm() async {
//   try {
//     final res = await http.post(
//       Uri.parse("$baseUrl/api/admin/alarm/off"),
//       headers: authHeaders,
//       body: jsonEncode({}),
//     );

//     final data = jsonDecode(res.body);

//     if (res.statusCode == 200 && data["success"] == true) {
//       await refreshAll();

//       if (!mounted) return;

//       ScaffoldMessenger.of(context).showSnackBar(
//         SnackBar(content: Text(data["message"] ?? "Đã tắt cảnh báo")),
//       );
//     } else {
//       if (!mounted) return;

//       ScaffoldMessenger.of(context).showSnackBar(
//         SnackBar(content: Text(data["message"] ?? "Không tắt được cảnh báo")),
//       );
//     }
//   } catch (e) {
//     print("turnOffAlarm error: $e");

//     if (!mounted) return;

//     ScaffoldMessenger.of(context).showSnackBar(
//       const SnackBar(content: Text("Lỗi khi tắt cảnh báo")),
//     );
//   }
// }
Future<void> disablePushNotifications() async {
  try {
    final res = await http.patch(
      Uri.parse("$baseUrl/api/device-tokens/disable-all"),
      headers: authHeaders,
    );

    print("DISABLE PUSH STATUS: ${res.statusCode}");
    print("DISABLE PUSH BODY: ${res.body}");

    final data = jsonDecode(res.body);

    if (res.statusCode == 200 && data["success"] == true) {
      setState(() {
        pushEnabled = false;
      });

      if (!mounted) return;

      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text("Đã tắt thông báo điện thoại")),
      );
    } else {
      if (!mounted) return;

      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(data["message"] ?? "Không tắt được thông báo")),
      );
    }
  } catch (e) {
    print("disablePushNotifications error: $e");

    if (!mounted) return;

    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text("Lỗi khi tắt thông báo")),
    );
  }
}
Future<void> enablePushNotifications() async {
  try {
    final settings = await _fcm.requestPermission(
      alert: true,
      badge: true,
      sound: true,
    );

    print("PUSH PERMISSION: ${settings.authorizationStatus}");

    final token = await _fcm.getToken();

    if (token == null) {
      if (!mounted) return;

      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text("Không lấy được device token")),
      );
      return;
    }

    currentDeviceToken = token;

    await registerDeviceToken(token);

    setState(() {
      pushEnabled = true;
    });

    if (!mounted) return;

    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text("Đã bật thông báo điện thoại")),
    );
  } catch (e) {
    print("enablePushNotifications error: $e");

    if (!mounted) return;

    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text("Lỗi khi bật thông báo")),
    );
  }
}
Future<void> togglePushNotification(bool value) async {
  if (pushLoading) return;

  setState(() {
    pushLoading = true;
  });

  if (value) {
    await enablePushNotifications();
  } else {
    await disablePushNotifications();
  }

  if (!mounted) return;

  setState(() {
    pushLoading = false;
  });
}
Future<void> setCurrentSafeLocation() async {
  if (!isAdmin) {
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(
        content: Text("Chỉ admin mới được đặt vị trí chuẩn"),
      ),
    );
    return;
  }

  final lat = toDoubleValue(currentGps?["gps_lat"]);
  final lng = toDoubleValue(currentGps?["gps_lng"]);

  if (lat == null || lng == null) {
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(
        content: Text("Chưa có GPS hiện tại từ két"),
      ),
    );
    return;
  }

  final address = await getAddressFromLatLng(lat, lng);

  if (!mounted) return;

  final confirm = await showDialog<bool>(
    context: context,
    builder: (context) {
      return AlertDialog(
        title: const Text("Đặt vị trí chuẩn của két"),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              "Vị trí hiện tại của két:",
              style: TextStyle(fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 8),
            Text("Lat: $lat"),
            Text("Lng: $lng"),
            const SizedBox(height: 12),
            const Text(
              "Địa điểm:",
              style: TextStyle(fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 8),
            Text(address),
            const SizedBox(height: 12),
            const Text(
              "Bạn có muốn gán vị trí này làm vị trí chuẩn không?",
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () async {
              final uri = Uri.parse(
                "https://www.google.com/maps/search/?api=1&query=$lat,$lng",
              );

              await launchUrl(
                uri,
                mode: LaunchMode.externalApplication,
              );
            },
            child: const Text("Xem bản đồ"),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text("Huỷ"),
          ),
          ElevatedButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text("Xác nhận"),
          ),
        ],
      );
    },
  );

  if (confirm != true) return;

  try {
    final res = await http.patch(
      Uri.parse("$baseUrl/api/admin/location/config"),
      headers: authHeaders,
      body: jsonEncode({
        "base_lat": lat,
        "base_lng": lng,
        "allowed_radius_m": int.tryParse(
              configValue("gps_allowed_radius_m"),
            ) ??
            50,
      }),
    );

    print("SET LOCATION STATUS: ${res.statusCode}");
    print("SET LOCATION BODY: ${res.body}");

    if (!mounted) return;

    if (res.statusCode == 200) {
      final data = jsonDecode(res.body);

      if (data["success"] == true) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text("Đã đặt vị trí chuẩn: $address"),
          ),
        );

        await fetchLocationConfig();
      } else {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(data["message"] ?? "Đặt vị trí thất bại"),
          ),
        );
      }
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text("Lỗi HTTP ${res.statusCode}"),
        ),
      );
    }
  } catch (e) {
    print("setCurrentSafeLocation error: $e");

    if (!mounted) return;

    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text("Lỗi đặt vị trí: $e"),
      ),
    );
  }
}
Future<void> fetchLocationConfig() async {
  if (!isAdmin) return;

  try {
    final res = await http.get(
      Uri.parse("$baseUrl/api/admin/location/config"),
      headers: authHeaders,
    );

    print("LOCATION CONFIG STATUS: ${res.statusCode}");
    print("LOCATION CONFIG BODY: ${res.body}");

    if (res.statusCode != 200) return;

    final data = jsonDecode(res.body);

    if (!mounted) return;

    if (data["success"] == true) {
      setState(() {
        locationConfig = data["data"];
      });
    }
  } catch (e) {
    print("fetchLocationConfig error: $e");
  }
}
Future<void> fetchPendingUsers() async {
  final res = await http.get(
    Uri.parse("$baseUrl/api/admin/users/pending"),
    headers: authHeaders,
  );

  final data = jsonDecode(res.body);

  if (res.statusCode == 200 && data["success"] == true) {
    setState(() {
      pendingUsers = data["data"] ?? [];
    });
  }
}

Future<void> approveUser(int id, bool allowSms) async {
  final res = await http.patch(
    Uri.parse("$baseUrl/api/admin/users/$id/approve"),
    headers: authHeaders,
    body: jsonEncode({
      "allow_sms": allowSms,
    }),
  );

  final data = jsonDecode(res.body);

  if (!mounted) return;

  ScaffoldMessenger.of(context).showSnackBar(
    SnackBar(
      content: Text(data["message"] ?? "Đã duyệt tài khoản"),
    ),
  );

  await fetchPendingUsers();
  await fetchSmsReceivers();();
}
Future<void> rejectUser(int id) async {
  final res = await http.patch(
    Uri.parse("$baseUrl/api/admin/users/$id/reject"),
    headers: authHeaders,
  );

  final data = jsonDecode(res.body);

  if (!mounted) return;

  ScaffoldMessenger.of(context).showSnackBar(
    SnackBar(
      content: Text(data["message"] ?? "Đã từ chối tài khoản"),
    ),
  );

  await fetchPendingUsers();
}
Future<void> deleteUser(int id) async {
  final confirm = await showDialog<bool>(
    context: context,
    builder: (_) => AlertDialog(
      title: const Text("Xoá tài khoản"),
      content: const Text("Bạn có chắc muốn xoá tài khoản user này không?"),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context, false),
          child: const Text("Huỷ"),
        ),
        FilledButton(
          onPressed: () => Navigator.pop(context, true),
          child: const Text("Xoá"),
        ),
      ],
    ),
  );

  if (confirm != true) return;

  try {
    final res = await http.delete(
      Uri.parse("$baseUrl/api/admin/users/$id"),
      headers: authHeaders,
    );

    final data = jsonDecode(res.body);

    if (!mounted) return;

    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(data["message"] ?? "Đã xoá user"),
      ),
    );
    await fetchPendingUsers();
    await fetchSmsReceivers();
    await fetchUsers();
  } catch (e) {
    if (!mounted) return;

    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text("Lỗi xoá user: $e"),
      ),
    );
  }
}
Future<void> fetchUsers() async {
  try {
    if (!isAdmin) return;

    setState(() {
      usersLoading = true;
    });

    final res = await http.get(
      Uri.parse("$baseUrl/api/admin/users"),
      headers: authHeaders,
    );

    print("USERS STATUS: ${res.statusCode}");
    print("USERS BODY: ${res.body}");

    final data = jsonDecode(res.body);

    if (res.statusCode == 200 && data["success"] == true) {
      setState(() {
        users = data["data"] ?? [];
      });
    }
  } catch (e) {
    print("fetchUsers error: $e");
  } finally {
    if (mounted) {
      setState(() {
        usersLoading = false;
      });
    }
  }
}
  @override
@override
Widget build(BuildContext context) {
  final pages = <Widget>[];
  final destinations = <NavigationDestination>[];

  void addPage({
    required Widget page,
    required IconData icon,
    required String label,
    Widget? customIcon,
  }) {
    pages.add(page);
    destinations.add(
      NavigationDestination(
        icon: customIcon ?? Icon(icon),
        label: label,
      ),
    );
  }

  // =========================
  // USER + ADMIN đều thấy
  // =========================
  addPage(
    page: dashboardView(),
    icon: Icons.dashboard,
    label: "Tổng quan",
  );

  // =========================
  // CHỈ ADMIN THẤY
  // =========================
  if (isAdmin) {
    addPage(
      page: eventsView(),
      icon: Icons.history,
      label: "Lịch sử",
    );

    addPage(
      page: smsView(),
      icon: Icons.sms,
      label: "SMS",
    );

    addPage(
      page: authMethodsView(),
      icon: Icons.fingerprint,
      label: "Xác thực",
    );
  }

  // =========================
  // USER + ADMIN đều thấy thông báo
  // =========================
  addPage(
    page: notificationsView(),
    icon: Icons.notifications,
    label: "Thông báo",
    customIcon: Badge(
      isLabelVisible: unreadCount > 0,
      label: Text(unreadCount.toString()),
      child: const Icon(Icons.notifications),
    ),
  );

  // =========================
  // USER + ADMIN đều thấy cài đặt
  // Nhưng bên trong settingsView() sẽ ẩn mục admin
  // =========================
  addPage(
    page: settingsView(),
    icon: Icons.settings,
    label: "Cài đặt",
  );

  final safeIndex =
      tabIndex >= pages.length ? 0 : tabIndex;

  return Scaffold(
    backgroundColor: const Color.fromARGB(255, 178, 218, 234),

    appBar: AppBar(
      backgroundColor: Colors.black87,
      centerTitle: true,
      toolbarHeight: 90,
      elevation: 8,
      title: Text(
        "SMART SAFE",
        style: TextStyle(
          fontSize: 50,
          fontWeight: FontWeight.bold,
          foreground: Paint()
            ..shader = LinearGradient(
              colors: [
                Colors.yellow.shade400,
                Colors.orange.shade700,
              ],
              begin: Alignment.topLeft,
              end: Alignment.bottomRight,
            ).createShader(
              const Rect.fromLTWH(0, 0, 300, 70),
            ),
          shadows: [
            Shadow(
              offset: const Offset(4, 4),
              blurRadius: 6,
              color: Colors.orange.shade900.withOpacity(0.6),
            ),
            Shadow(
              offset: const Offset(-3, -3),
              blurRadius: 3,
              color: Colors.yellow.shade600.withOpacity(0.5),
            ),
          ],
        ),
      ),
      actions: [
        IconButton(
          onPressed: refreshAll,
          icon: const Icon(Icons.refresh),
        ),
        IconButton(
          icon: const Icon(Icons.logout),
          onPressed: () {
            Navigator.pushReplacement(
              context,
              MaterialPageRoute(
                builder: (_) => const LoginPage(),
              ),
            );
          },
        ),
      ],
    ),

    body: pages[safeIndex],

    bottomNavigationBar: NavigationBar(
      selectedIndex: safeIndex,
      onDestinationSelected: (i) {
        setState(() {
          tabIndex = i;
        });
      },
      destinations: destinations,
    ),
  );
}
Future<void> showWifiConfigDialog() async {
  final ssidCtrl = TextEditingController();
  final passCtrl = TextEditingController();

  try {
    final res = await http.get(
      Uri.parse("$baseUrl/api/admin/wifi-config"),
      headers: authHeaders,
    );

    final data = jsonDecode(res.body);

    if (res.statusCode == 200 && data["success"] == true) {
      ssidCtrl.text = data["data"]["wifi_ssid"] ?? "";
    }
  } catch (e) {
    print("fetch wifi config error: $e");
  }

  if (!mounted) return;

  showDialog(
    context: context,
    builder: (_) {
      return AlertDialog(
        title: const Text("Cấu hình WiFi ESP32"),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: ssidCtrl,
              decoration: const InputDecoration(
                labelText: "Tên WiFi SSID",
                prefixIcon: Icon(Icons.wifi),
              ),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: passCtrl,
              obscureText: true,
              decoration: const InputDecoration(
                labelText: "Mật khẩu WiFi",
                prefixIcon: Icon(Icons.lock),
                helperText: "Để trống nếu không đổi mật khẩu",
              ),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text("Huỷ"),
          ),
          FilledButton(
            onPressed: () async {
              await saveWifiConfig(
                ssidCtrl.text.trim(),
                passCtrl.text.trim(),
              );

              if (!mounted) return;

              Navigator.pop(context);
            },
            child: const Text("Lưu"),
          ),
        ],
      );
    },
  );
}
Future<void> saveWifiConfig(String ssid, String password) async {
  try {
    if (ssid.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text("SSID không được rỗng")),
      );
      return;
    }

    final res = await http.post(
      Uri.parse("$baseUrl/api/admin/wifi-config"),
      headers: authHeaders,
      body: jsonEncode({
        "wifi_ssid": ssid,
        "wifi_password": password,
      }),
    );

    print("SAVE WIFI STATUS: ${res.statusCode}");
    print("SAVE WIFI BODY: ${res.body}");

    final data = jsonDecode(res.body);

    if (!mounted) return;

    if (res.statusCode == 200 && data["success"] == true) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(data["message"] ?? "Đã lưu WiFi")),
      );
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(data["message"] ?? "Không lưu được WiFi")),
      );
    }
  } catch (e) {
    print("saveWifiConfig error: $e");

    if (!mounted) return;

    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text("Lỗi khi lưu WiFi")),
    );
  }
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
                colors: [Color.fromARGB(255, 176, 223, 218), Color(0xff4db6ac)],
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
          const SizedBox(height: 12),

          // SizedBox(
          //   height: 54,
          //   child: FilledButton.icon(
          //     style: FilledButton.styleFrom(
          //       backgroundColor: Colors.red,
          //     ),
          //     onPressed: turnOffAlarm,
          //     icon: const Icon(Icons.volume_off),
          //     label: const Text("Tắt cảnh báo"),
          //   ),
          // ),
        ],
      ),
    );
  }

//   Widget eventsView() {
//   return Column(
//     children: [
//       // Header + Trash icon
//       Padding(
//         padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
//         child: Row(
//           mainAxisAlignment: MainAxisAlignment.spaceBetween,
//           children: [
//             const Text(
//               "Lịch sử sự kiện",
//               style: TextStyle(fontSize: 22, fontWeight: FontWeight.bold),
//             ),
//             IconButton(
//               icon: Icon(editingMode ? Icons.close : Icons.delete),
//               onPressed: () {
//                 setState(() {
//                   editingMode = !editingMode;
//                   if (!editingMode) selectedEvents.clear();
//                 });
//               },
//             ),
//           ],
//         ),
//       ),

//       // Remove / Remove All /Restore buttons khi editingMode
//       if (editingMode)
//         Padding(
//           padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
//           child: Column(
//             children: [
//               Row(
//                 children: [
//                   Expanded(
//                     child: ElevatedButton.icon(
//                       icon: const Icon(Icons.delete_forever),
//                       label: const Text("Remove Selected"),
//                       onPressed: () {
//                         final idsToRemove = selectedEvents.entries
//                             .where((e) => e.value)
//                             .map((e) => e.key)
//                             .toList();
//                         removeEvents(idsToRemove);
//                       },
//                     ),
//                   ),
//                   const SizedBox(width: 8),
//                   Expanded(
//                     child: ElevatedButton.icon(
//                       icon: const Icon(Icons.delete_sweep),
//                       label: const Text("Remove All"),
//                       onPressed: () {
//                         removeEvents(
//                           events.map((e) => int.parse(e["id"].toString())).toList(),
//                         );
//                       },
//                     ),
//                   ),
//                 ],
//               ),

//               const SizedBox(height: 8),

//               Row(
//                 children: [
//                   Expanded(
//                     child: ElevatedButton.icon(
//                       style: ElevatedButton.styleFrom(
//                         backgroundColor: Colors.green,
//                       ),
//                       icon: const Icon(Icons.restore),
//                       label: const Text("Restore Selected"),
//                       onPressed: () {
//                         final idsToRestore = selectedEvents.entries
//                             .where((e) => e.value)
//                             .map((e) => e.key)
//                             .toList();
//                         restoreEvents(idsToRestore);
//                       },
//                     ),
//                   ),
//                   const SizedBox(width: 8),
//                   Expanded(
//                     child: ElevatedButton.icon(
//                       style: ElevatedButton.styleFrom(
//                         backgroundColor: Colors.blue,
//                       ),
//                       icon: const Icon(Icons.restore_from_trash),
//                       label: const Text("Restore All"),
//                       onPressed: () {
//                         restoreEvents(
//                           events.map((e) => int.parse(e["id"].toString())).toList(),
//                         );
//                       },
//                     ),
//                   ),
//                 ],
//               ),
//             ],
//           ),
//         ),

//       Expanded(
//         child: RefreshIndicator(
//           onRefresh: fetchEvents,
//           child: ListView.builder(
//             padding: const EdgeInsets.all(16),
//             itemCount: events.length,
//             itemBuilder: (context, index) {
//               final e = events[index];
//               final checked = selectedEvents[e["id"]] ?? false;

//               return Card(
//                 child: ListTile(
//                   leading: editingMode
//                       ? Checkbox(
//                           value: checked,
//                           onChanged: (val) {
//                             setState(() {
//                               selectedEvents[e["id"]] = val ?? false;
//                             });
//                           },
//                         )
//                       : Icon(
//                           eventIcon(e["event_type"]?.toString() ?? ""),
//                           color: eventColor(e["event_type"]?.toString() ?? ""),
//                         ),
//                   title: Text(e["event_type"]?.toString() ?? ""),
//                   subtitle: Text(
//                       "${e["message"] ?? ""}\n${formatTime(e["created_at"]?.toString())}"),
//                   trailing: editingMode
//                       ? null
//                       : IconButton(
//                           icon: const Icon(Icons.map, color: Colors.teal),
//                           onPressed: () {
//                             openMap(
//                               e["gps_lat"],
//                               e["gps_lng"],
//                             );
//                           },
//                         ),
//                   isThreeLine: true,
//                 ),
//               );
//             },
//           ),
//         ),
//       ),
//     ],
//   );
// }
Widget eventsView() {
  return Column(
    children: [
      // Header + Trash icon + Toggle Deleted
      Padding(
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            const Text(
              "Lịch sử sự kiện",
              style: TextStyle(fontSize: 22, fontWeight: FontWeight.bold),
            ),
            Row(
              children: [
                TextButton(
                  onPressed: () {
                    setState(() {
                      showDeletedEvents = !showDeletedEvents;
                    });
                    if (showDeletedEvents) {
                      fetchDeletedEvents();
                    } else {
                      fetchEvents();
                    }
                  },
                  child: Text(
                    showDeletedEvents ? "Xem Active" : "Xem Deleted",
                    style: const TextStyle(fontSize: 14),
                  ),
                ),
                IconButton(
                  icon: Icon(editingMode ? Icons.close : Icons.delete),
                  onPressed: () {
                    setState(() {
                      editingMode = !editingMode;
                      if (!editingMode) selectedEvents.clear();
                    });
                  },
                ),
              ],
            ),
          ],
        ),
      ),

      // Remove / Remove All / Restore buttons khi editingMode
      if (editingMode)
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
          child: Column(
            children: [
              Row(
                children: [
                  if (!showDeletedEvents) ...[
                    // Xoá khi đang xem active events
                    Expanded(
                      child: ElevatedButton.icon(
                        icon: const Icon(Icons.delete_forever),
                        label: const Text("Remove Selected"),
                        onPressed: () {
                          final idsToRemove = selectedEvents.entries
                              .where((e) => e.value)
                              .map((e) => e.key)
                              .toList();
                          removeEvents(idsToRemove);
                        },
                      ),
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: ElevatedButton.icon(
                        icon: const Icon(Icons.delete_sweep),
                        label: const Text("Remove All"),
                        onPressed: () {
                          removeEvents(
                            events.map((e) => int.parse(e["id"].toString())).toList(),
                          );
                        },
                      ),
                    ),
                  ] else ...[
                    // Restore khi đang xem deleted events
                    Expanded(
                      child: ElevatedButton.icon(
                        icon: const Icon(Icons.restore),
                        label: const Text("Restore Selected"),
                        onPressed: () {
                          final idsToRestore = selectedEvents.entries
                              .where((e) => e.value)
                              .map((e) => e.key)
                              .toList();
                          restoreEvents(idsToRestore);
                        },
                      ),
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: ElevatedButton.icon(
                        icon: const Icon(Icons.restore_from_trash),
                        label: const Text("Restore All"),
                        onPressed: () {
                          restoreEvents(
                            events.map((e) => int.parse(e["id"].toString())).toList(),
                          );
                        },
                      ),
                    ),
                  ],
                ],
              ),
            ],
          ),
        ),

      Expanded(
        child: RefreshIndicator(
          onRefresh: showDeletedEvents ? fetchDeletedEvents : fetchEvents,
          child: ListView.builder(
            padding: const EdgeInsets.all(16),
            itemCount: events.length,
            itemBuilder: (context, index) {
              final e = events[index];
              final checked = selectedEvents[e["id"]] ?? false;

              return Card(
                child: ListTile(
                  leading: editingMode
                      ? Checkbox(
                          value: checked,
                          onChanged: (val) {
                            setState(() {
                              selectedEvents[e["id"]] = val ?? false;
                            });
                          },
                        )
                      : Icon(
                          eventIcon(e["event_type"]?.toString() ?? ""),
                          color: eventColor(e["event_type"]?.toString() ?? ""),
                        ),
                  title: Text(e["event_type"]?.toString() ?? ""),
                  subtitle: Text(
                      "${e["message"] ?? ""}\n${formatTime(e["created_at"]?.toString())}"),
                  trailing: editingMode
                      ? null
                      : IconButton(
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
              );
            },
          ),
        ),
      ),
    ],
  );
}
  Widget smsView() {
  if (!isAdmin) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Text(
          "Bạn không có quyền quản lý SMS",
          style: TextStyle(
            fontSize: 16,
            color: Colors.grey.shade700,
          ),
        ),
      ),
    );
  }

  return ListView(
    padding: const EdgeInsets.all(16),
    children: [
      Positioned.fill(
        child: Opacity(
          opacity: 0.55,
          child: SvgPicture.asset(
            "assets/svg/auth_background.svg",
            fit: BoxFit.cover,
          ),
        ),
      ),
      // =====================================================
      // TÀI KHOẢN CHỜ DUYỆT
      // =====================================================
      _sectionTitle("Tài khoản chờ duyệt"),

      const SizedBox(height: 8),

      if (pendingUsersLoading) ...[
        const Center(
          child: Padding(
            padding: EdgeInsets.all(16),
            child: CircularProgressIndicator(),
          ),
        ),
      ] else if (pendingUsers.isEmpty) ...[
        Card(
          child: ListTile(
            leading: const Icon(Icons.verified_user, color: Colors.green),
            title: const Text("Không có tài khoản chờ duyệt"),
            subtitle: const Text("Các user mới đăng ký sẽ hiện ở đây"),
          ),
        ),
      ] else ...[
        ...pendingUsers.map<Widget>((u) {
          return Card(
            child: Padding(
              padding: const EdgeInsets.symmetric(vertical: 8),
              child: ListTile(
                leading: const CircleAvatar(
                  backgroundColor: Color(0xFFE0F2F1),
                  child: Icon(Icons.person, color: Colors.teal),
                ),

                title: Text(
                  u["full_name"]?.toString() ?? "Người dùng mới",
                  style: const TextStyle(
                    fontWeight: FontWeight.bold,
                  ),
                ),

                subtitle: Text(
                  "Tên đăng nhập: ${u["username"]?.toString() ?? ""}\n"
                  "SĐT: ${u["phone"]?.toString() ?? ""}\n"
                  "Trạng thái: ${u["status"]?.toString() ?? "pending"}",
                ),

                isThreeLine: true,

                trailing: Wrap(
                  spacing: 4,
                  children: [
                    IconButton(
                      tooltip: "Duyệt",
                      icon: const Icon(Icons.check_circle, color: Colors.green),
                      onPressed: () {
                        approveUser(u["id"], true);
                      },
                    ),

                    // IconButton(
                    //   tooltip: "Duyệt tài khoản",
                    //   icon: const Icon(Icons.check_circle, color: Colors.blue),
                    //   onPressed: () {
                    //     approveUser(u["id"], false);
                    //   },
                    // ),

                    IconButton(
                      tooltip: "Từ chối",
                      icon: const Icon(Icons.cancel, color: Colors.orange),
                      onPressed: () {
                        rejectUser(u["id"]);
                      },
                    ),

                    // IconButton(
                    //   tooltip: "Xoá user",
                    //   icon: const Icon(Icons.delete_forever, color: Colors.red),
                    //   onPressed: () {
                    //     deleteUser(u["id"]);
                    //   },
                    // ),
                  ],
                ),
              ),
            ),
          );
        }).toList(),
      ],

      const SizedBox(height: 24),

      _sectionTitle("Danh sách tài khoản user"),

      const SizedBox(height: 8),

      if (usersLoading) ...[
        const Center(
          child: Padding(
            padding: EdgeInsets.all(16),
            child: CircularProgressIndicator(),
          ),
        ),
      ] else if (users.where((u) => u["role"] != "admin").isEmpty) ...[
        Card(
          child: ListTile(
            leading: const Icon(Icons.people, color: Colors.grey),
            title: const Text("Chưa có user"),
            subtitle: const Text("User đã duyệt sẽ hiện ở đây"),
          ),
        ),
      ] else ...[
        ...users
            .where((u) => u["role"] != "admin")
            .map<Widget>((u) {
          return Card(
            child: ListTile(
              leading: const CircleAvatar(
                backgroundColor: Color(0xFFE3F2FD),
                child: Icon(Icons.person, color: Colors.blue),
              ),
              title: Text(
                u["full_name"]?.toString() ?? "Không tên",
                style: const TextStyle(
                  fontWeight: FontWeight.bold,
                ),
              ),
              subtitle: Text(
                "Tên đăng nhập: ${u["username"]?.toString() ?? ""}\n"
                "SĐT: ${u["phone"]?.toString() ?? ""}\n"
                "Trạng thái: ${u["status"]?.toString() ?? ""}",
              ),
              isThreeLine: true,
              trailing: IconButton(
                tooltip: "Xoá user",
                icon: const Icon(Icons.delete_forever, color: Colors.red),
                onPressed: () {
                  deleteUser(u["id"]);
                },
              ),
            ),
          );
        }).toList(),
      ],
      // =====================================================
      // DANH SÁCH SỐ NHẬN SMS
      // =====================================================
      _sectionTitle("Số điện thoại nhận SMS"),

      const SizedBox(height: 8),

      FilledButton.icon(
        onPressed: addSmsReceiver,
        icon: const Icon(Icons.add),
        label: const Text("Thêm số SMS"),
      ),

      const SizedBox(height: 12),

      if (smsReceivers.isEmpty)
        Card(
          child: ListTile(
            leading: const Icon(Icons.phone_disabled, color: Colors.grey),
            title: const Text("Chưa có số SMS"),
            subtitle: const Text("Các số được duyệt nhận SMS sẽ hiện ở đây"),
          ),
        )
      else
        ...smsReceivers.map(
          (s) => Card(
            child: ListTile(
              leading: const Icon(Icons.phone, color: Colors.teal),
              title: Text(
                s["name"]?.toString() ?? "Không tên",
                style: const TextStyle(
                  fontWeight: FontWeight.w600,
                ),
              ),
              subtitle: Text(
                s["phone"]?.toString() ?? "",
              ),
              trailing: IconButton(
                tooltip: "Xoá số SMS",
                icon: const Icon(Icons.delete, color: Colors.red),
                onPressed: () => deleteSmsReceiver(s["id"]),
              ),
            ),
          ),
        ),
    ],
  );
}
  Future<void> showAlertConfigDialog() async {
    final maxWrongController = TextEditingController(
      text: configValue("max_wrong_password").isEmpty
          ? "5"
          : configValue("max_wrong_password"),
    );

    final gpsRadiusController = TextEditingController(
      text: configValue("gps_allowed_radius_m").isEmpty
          ? "50"
          : configValue("gps_allowed_radius_m"),
    );

    bool vibrationEnabled =
        configValue("alert_vibration_enabled") != "0";

    bool doorEnabled =
        configValue("alert_door_enabled") != "0";

    bool flameEnabled =
        configValue("flame_alert_enabled") != "0";

    bool gpsEnabled =
        configValue("gps_alert_enabled") != "0";



    await showDialog(
      context: context,
      builder: (dialogContext) {
        return StatefulBuilder(
          builder: (context, setDialogState) {
            return AlertDialog(
              title: const Text("Cấu hình cảnh báo"),
              content: SingleChildScrollView(
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    SwitchListTile(
                      title: const Text("Cảnh báo rung"),
                      value: vibrationEnabled,
                      onChanged: (v) {
                        setDialogState(() {
                          vibrationEnabled = v;
                        });
                      },
                    ),

                    SwitchListTile(
                      title: const Text("Cảnh báo cửa mở trái phép"),
                      value: doorEnabled,
                      onChanged: (v) {
                        setDialogState(() {
                          doorEnabled = v;
                        });
                      },
                    ),

                    SwitchListTile(
                      title: const Text("Cảnh báo lửa"),
                      value: flameEnabled,
                      onChanged: (v) {
                        setDialogState(() {
                          flameEnabled = v;
                        });
                      },
                    ),

                    SwitchListTile(
                      title: const Text("Cảnh báo di chuyển GPS"),
                      value: gpsEnabled,
                      onChanged: (v) {
                        setDialogState(() {
                          gpsEnabled = v;
                        });
                      },
                    ),

                    const SizedBox(height: 12),

                    TextField(
                      controller: maxWrongController,
                      keyboardType: TextInputType.number,
                      decoration: const InputDecoration(
                        labelText: "Số lần nhập sai tối đa",
                        border: OutlineInputBorder(),
                      ),
                    ),

                    const SizedBox(height: 12),
                    TextField(
                      controller: gpsRadiusController,
                      keyboardType: TextInputType.number,
                      decoration: const InputDecoration(
                        labelText: "Bán kính GPS cho phép mét",
                        border: OutlineInputBorder(),
                      ),
                    ),
                  ],
                ),
              ),
              actions: [
                TextButton(
                  onPressed: () {
                    Navigator.pop(dialogContext);
                  },
                  child: const Text("Huỷ"),
                ),

                FilledButton(
                  onPressed: () async {
                    await updateAlertConfig({
                      "max_wrong_password": maxWrongController.text.trim(),
                      "gps_allowed_radius_m": gpsRadiusController.text.trim(),
                      "alert_vibration_enabled": vibrationEnabled ? "1" : "0",
                      "alert_door_enabled": doorEnabled ? "1" : "0",
                      "flame_alert_enabled": flameEnabled ? "1" : "0",
                      "gps_alert_enabled": gpsEnabled ? "1" : "0",
                    });

                    if (!mounted) return;

                    Navigator.pop(dialogContext);
                  },
                  child: const Text("Lưu"),
                ),
              ],
            );
          },
        );
      },
    );

    maxWrongController.dispose();
    gpsRadiusController.dispose();
  }
  Future<void> updateAlertConfig(Map<String, String> values) async {
  try {
    final res = await http.patch(
      Uri.parse("$baseUrl/api/admin/config/bulk"),
      headers: authHeaders,
      body: jsonEncode({
        "values": values,
      }),
    );

    final data = jsonDecode(res.body);

    if (!mounted) return;

    if (res.statusCode == 200 && data["success"] == true) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text("Đã lưu cấu hình cảnh báo"),
        ),
      );

      await fetchConfig();
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(data["message"] ?? "Lưu cấu hình thất bại"),
        ),
      );
    }
  } catch (e) {
    if (!mounted) return;

    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text("Lỗi lưu cấu hình: $e"),
      ),
    );
  }
}
Widget _sectionTitle(String text) {
  return Padding(
    padding: const EdgeInsets.only(bottom: 12, top: 8),
    child: Text(
      text,
      style: const TextStyle(
        fontSize: 22,
        fontWeight: FontWeight.bold,
        color: Colors.black87,
      ),
    ),
  );
}
Widget statusCard(
  String title,
  String value,
  IconData icon,
  Color color,
) {
  return Container(
    padding: const EdgeInsets.all(18),
    decoration: BoxDecoration(
      color: color.withOpacity(0.12),
      borderRadius: BorderRadius.circular(24),
      border: Border.all(
        color: color.withOpacity(0.25),
      ),
      boxShadow: [
        BoxShadow(
          color: color.withOpacity(0.08),
          blurRadius: 12,
          offset: const Offset(0, 6),
        ),
      ],
    ),
    child: Column(
      mainAxisAlignment: MainAxisAlignment.center,
      children: [
        Icon(
          icon,
          color: color,
          size: 38,
        ),
        const SizedBox(height: 8),
        Text(
          title,
          textAlign: TextAlign.center,
          style: const TextStyle(
            fontSize: 14,
            color: Colors.black87,
          ),
        ),
        const SizedBox(height: 6),
        Text(
          value,
          textAlign: TextAlign.center,
          maxLines: 2,
          overflow: TextOverflow.ellipsis,
          style: TextStyle(
            color: color,
            fontWeight: FontWeight.bold,
            fontSize: 18,
          ),
        ),
      ],
    ),
  );
}
Widget settingsView() {
  return Stack(
    children: [
      Positioned.fill(
        child: Opacity(
          opacity: 0.55,
          child: SvgPicture.asset(
            "assets/svg/settings_bg.svg",
            fit: BoxFit.cover,
          ),
        ),
      ),

      ListView(
        padding: const EdgeInsets.all(16),
        children: [
          _sectionTitle("Cài đặt tài khoản"),

          Card(
            color: Colors.white.withOpacity(0.88),
            child: ListTile(
              leading: const Icon(Icons.lock_reset, color: Colors.teal),
              title: const Text("Đổi mật khẩu"),
              subtitle: const Text("Thay đổi mật khẩu đăng nhập tài khoản"),
              trailing: const Icon(Icons.chevron_right),
              onTap: () {
                Navigator.push(
                  context,
                  MaterialPageRoute(
                    builder: (_) => ChangePasswordPage(
                      token: widget.token,
                    ),
                  ),
                );
              },
            ),
          ),

          Card(
            color: Colors.white.withOpacity(0.88),
            child: SwitchListTile(
              secondary: Icon(
                pushEnabled
                    ? Icons.notifications_active
                    : Icons.notifications_off,
                color: pushEnabled ? Colors.green : Colors.grey,
              ),
              title: const Text("Thông báo điện thoại"),
              subtitle: Text(
                pushEnabled
                    ? "Đang nhận thông báo cảnh báo từ két"
                    : "Đã tắt thông báo trên điện thoại",
              ),
              value: pushEnabled,
              onChanged: pushLoading ? null : togglePushNotification,
            ),
          ),

          if (isAdmin) ...[
            const SizedBox(height: 20),

            _sectionTitle("Cài đặt hệ thống"),

            Card(
              color: Colors.white.withOpacity(0.88),
              child: ListTile(
                leading: const Icon(Icons.pin, color: Colors.orange),
                title: const Text("Đổi mật khẩu keypad"),
                subtitle: Text(
                  "Hiện tại: ${configValue("keypad_password").isEmpty ? "Chưa cấu hình" : configValue("keypad_password")}",
                ),
                trailing: const Icon(Icons.chevron_right),
                onTap: changeKeypadPassword,
              ),
            ),

            Card(
              color: Colors.white.withOpacity(0.88),
              child: ListTile(
                leading: const Icon(Icons.settings, color: Colors.blue),
                title: const Text("Cấu hình cảnh báo"),
                subtitle: Text(
                  "Sai tối đa: ${configValue("max_wrong_password")}\n"
                  "Rung: ${configValue("alert_vibration_enabled") == "1" ? "Bật" : "Tắt"}\n"
                  "Cửa: ${configValue("alert_door_enabled") == "1" ? "Bật" : "Tắt"}\n"
                  "Lửa: ${configValue("flame_alert_enabled") == "1" ? "Bật" : "Tắt"}\n"
                  "GPS radius: ${configValue("gps_allowed_radius_m")}m",
                ),
                isThreeLine: true,
                trailing: const Icon(Icons.chevron_right),
                onTap: showAlertConfigDialog,
              ),
            ),

            Card(
              color: Colors.white.withOpacity(0.88),
              child: ListTile(
                leading: const Icon(Icons.wifi, color: Colors.blue),
                title: const Text("Cấu hình WiFi ESP32"),
                subtitle: const Text("Đổi tên WiFi cho két"),
                trailing: const Icon(Icons.chevron_right),
                onTap: showWifiConfigDialog,
              ),
            ),

            Card(
              color: Colors.white.withOpacity(0.88),
              child: ListTile(
                leading: const Icon(Icons.location_on, color: Colors.blue),
                title: const Text("Đặt vị trí hiện tại của két"),
                subtitle: Text(
                  currentGps == null || currentGps?["gps_lat"] == null
                      ? "Chưa có GPS từ két"
                      : "GPS hiện tại: ${currentGps?["gps_lat"]}, ${currentGps?["gps_lng"]}",
                ),
                trailing: const Icon(Icons.chevron_right),
                onTap: setCurrentSafeLocation,
              ),
            ),

            Card(
              color: Colors.white.withOpacity(0.88),
              child: ListTile(
                leading: const Icon(Icons.my_location, color: Colors.green),
                title: const Text("Vị trí chuẩn của két"),
                subtitle: Text(
                  locationConfig == null ||
                          locationConfig?["base_lat"] == null
                      ? "Chưa đặt vị trí chuẩn"
                      : "Lat: ${locationConfig?["base_lat"]}\n"
                          "Lng: ${locationConfig?["base_lng"]}\n"
                          "Bán kính: ${locationConfig?["allowed_radius_m"]}m",
                ),
              ),
            ),
          ],
        ],
      ),
    ],
  );
}

Widget authMethodsView() {
  return Stack(
    children: [
      RefreshIndicator(
        onRefresh: fetchAuthMethods,
        child: ListView(
          padding: const EdgeInsets.all(20),
          children: [
            const Text(
              "Quản lý RFID / Vân tay",
              style: TextStyle(
                fontSize: 28,
                fontWeight: FontWeight.bold,
                color: Colors.teal,
              ),
            ),
            const SizedBox(height: 24),
            Row(
              children: [
                Expanded(
                  child: gradientButton(
                    label: "Thêm RFID",
                    icon: Icons.credit_card,
                    onTap: () => enrollAuthMethod("RFID"),
                    startColor: Colors.teal,
                    endColor: Colors.blueAccent,
                  ),
                ),
                const SizedBox(width: 16),
                Expanded(
                  child: gradientButton(
                    label: "Thêm vân tay",
                    icon: Icons.fingerprint,
                    onTap: () => enrollAuthMethod("FINGERPRINT"),
                    startColor: Colors.orange,
                    endColor: Colors.deepOrangeAccent,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 32),
            if (authMethods.isEmpty)
              const Padding(
                padding: EdgeInsets.symmetric(vertical: 60),
                child: Center(
                  child: Text(
                    "Chưa có phương thức xác thực",
                    style: TextStyle(fontSize: 18, color: Colors.black38),
                  ),
                ),
              )
            else
              ...authMethods.map((a) {
                final type = a["method_type"]?.toString() ?? "";
                final value = a["method_value"]?.toString() ?? "";
                final name = a["user_name"]?.toString() ?? "";

                Color cardColor = type == "RFID" ? Colors.teal.shade50 : Colors.orange.shade50;
                IconData iconData = type == "RFID" ? Icons.credit_card : Icons.fingerprint;

                return Card(
                  elevation: 6,
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(20),
                  ),
                  color: cardColor,
                  margin: const EdgeInsets.symmetric(vertical: 8),
                  child: ListTile(
                    contentPadding: const EdgeInsets.symmetric(horizontal: 20, vertical: 14),
                    leading: CircleAvatar(
                      radius: 26,
                      backgroundColor: Colors.white,
                      child: Icon(iconData, color: type == "RFID" ? Colors.teal : Colors.orange, size: 30),
                    ),
                    title: Text(
                      name,
                      style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 18),
                    ),
                    subtitle: Text(
                      type == "RFID" ? "Thẻ RFID: $value" : "Vân tay ID: $value",
                      style: const TextStyle(fontSize: 14, color: Colors.black54),
                    ),
                    trailing: IconButton(
                      icon: const Icon(Icons.delete_forever, color: Colors.redAccent, size: 28),
                      tooltip: "Xóa phương thức",
                      onPressed: () async {
                        final confirm = await showDialog<bool>(
                          context: context,
                          builder: (_) => AlertDialog(
                            shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
                            title: const Text("Xác nhận xóa"),
                            content: Text("Xóa $type của $name?"),
                            actions: [
                              TextButton(
                                  onPressed: () => Navigator.pop(context, false),
                                  child: const Text("Hủy")),
                              TextButton(
                                  onPressed: () => Navigator.pop(context, true),
                                  child: const Text("Xóa")),
                            ],
                          ),
                        );
                        if (confirm == true) removeAuthMethod(type, value);
                      },
                    ),
                  ),
                );
              }),
          ],
        ),
      ),
    ],
  );
}

Widget gradientButton({
  required String label,
  required IconData icon,
  required VoidCallback onTap,
  Color startColor = Colors.teal,
  Color endColor = Colors.blue,
}) {
  return StatefulBuilder(
    builder: (context, setState) {
      bool pressed = false;

      return GestureDetector(
        onTapDown: (_) => setState(() => pressed = true),
        onTapUp: (_) {
          setState(() => pressed = false);
          onTap();
        },
        onTapCancel: () => setState(() => pressed = false),
        child: AnimatedContainer(
          duration: const Duration(milliseconds: 100),
          padding: const EdgeInsets.symmetric(vertical: 16),
          decoration: BoxDecoration(
            gradient: pressed
                ? LinearGradient(colors: [endColor.withOpacity(0.8), startColor.withOpacity(0.8)])
                : LinearGradient(colors: [startColor, endColor]),
            borderRadius: BorderRadius.circular(24),
            boxShadow: pressed
                ? []
                : [
                    BoxShadow(
                      color: startColor.withOpacity(0.35),
                      offset: const Offset(0, 4),
                      blurRadius: 6,
                    )
                  ],
          ),
          child: Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(icon, color: Colors.white),
              const SizedBox(width: 8),
              Text(
                label,
                style: const TextStyle(color: Colors.white, fontWeight: FontWeight.bold, fontSize: 16),
              ),
            ],
          ),
        ),
      );
    },
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

String formatTime(dynamic time) {
  if (time == null) return "";

  String text = time.toString().trim();
  if (text.isEmpty) return "";

  try {
    text = text.replaceFirst(" ", "T");

    DateTime dt;

    if (text.endsWith("Z") || RegExp(r'[+-]\d{2}:\d{2}$').hasMatch(text)) {
      dt = DateTime.parse(text).toLocal();
    } else {
      dt = DateTime.parse("${text}Z").toLocal();
    }

    return "${dt.day.toString().padLeft(2, '0')}/"
        "${dt.month.toString().padLeft(2, '0')}/"
        "${dt.year} "
        "${dt.hour.toString().padLeft(2, '0')}:"
        "${dt.minute.toString().padLeft(2, '0')}:"
        "${dt.second.toString().padLeft(2, '0')}";
  } catch (_) {
    return time.toString();
  }
}
String formatNotificationTime(dynamic time) {
  if (time == null) return "";

  final text = time.toString().trim();
  if (text.isEmpty) return "";

  try {
    final dt = DateTime.parse(text.replaceFirst(" ", "T"))
        .add(const Duration(hours: 14));

    return "${dt.day.toString().padLeft(2, '0')}/"
        "${dt.month.toString().padLeft(2, '0')}/"
        "${dt.year} "
        "${dt.hour.toString().padLeft(2, '0')}:"
        "${dt.minute.toString().padLeft(2, '0')}:"
        "${dt.second.toString().padLeft(2, '0')}";
  } catch (_) {
    return text;
  }
}
Widget notificationsView() {
  return Column(
    children: [
      Padding(
        padding: const EdgeInsets.all(16),
        child: Row(
          children: [
            const Expanded(
              child: Text(
                "Thông báo",
                style: TextStyle(
                  fontSize: 22,
                  fontWeight: FontWeight.bold,
                ),
              ),
            ),
            TextButton.icon(
              onPressed: notifications.isEmpty ? null : markAllNotificationsRead,
              icon: const Icon(Icons.done_all),
              label: const Text("Đọc tất cả"),
            ),
          ],
        ),
      ),

      Expanded(
        child: RefreshIndicator(
          onRefresh: fetchNotifications,
          child: notifications.isEmpty
              ? ListView(
                  children: const [
                    SizedBox(height: 120),
                    Center(
                      child: Text(
                        "Chưa có thông báo",
                        style: TextStyle(fontSize: 18, color: Colors.black54),
                      ),
                    ),
                  ],
                )
              : ListView.builder(
                  padding: const EdgeInsets.all(16),
                  itemCount: notifications.length,
                  itemBuilder: (context, index) {
                    final n = notifications[index];
                    final isRead = n["is_read"] == 1;

                    return Card(
                      color: isRead ? Colors.white : Colors.teal.shade50,
                      child: ListTile(
                        leading: Icon(
                          notificationIcon(n["type"]?.toString() ?? ""),
                          color: isRead ? Colors.grey : Colors.teal,
                        ),
                        title: Text(
                          n["title"]?.toString() ?? "",
                          style: TextStyle(
                            fontWeight:
                                isRead ? FontWeight.normal : FontWeight.bold,
                          ),
                        ),
                        subtitle: Text(
                          "${n["body"] ?? n["message"] ?? ""}\n${formatNotificationTime(n["created_at"])}"
                        ),
                        isThreeLine: true,
                        trailing: isRead
                            ? null
                            : IconButton(
                                icon: const Icon(Icons.done, color: Colors.green),
                                onPressed: () {
                                  markNotificationRead(
                                    int.parse(n["id"].toString()),
                                  );
                                },
                              ),
                      ),
                    );
                  },
                ),
        ),
      ),
    ],
  );
}

IconData notificationIcon(String type) {
  if (type.contains("FIRE") || type.contains("SMOKE") || type.contains("GAS")) {
    return Icons.local_fire_department;
  }

  if (type.contains("VIBRATION")) {
    return Icons.vibration;
  }

  if (type.contains("DOOR") || type.contains("INTRUSION")) {
    return Icons.door_back_door;
  }

  if (type.contains("UNLOCK")) {
    return Icons.lock_open;
  }

  if (type.contains("PASSWORD")) {
    return Icons.password;
  }

  return Icons.notifications;
}
}
class OtpDialog extends StatefulWidget {
  final int userId;
  final String otp;
  final String token;
  final VoidCallback onSuccess;

  const OtpDialog({
    super.key,
    required this.userId,
    required this.otp,
    required this.token,
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

    try {
      final res = await http.post(
        Uri.parse("$baseUrl/api/verify-otp-open"),
        headers: {
          "Content-Type": "application/json",
          "Authorization": "Bearer ${widget.token}",
        },
        body: jsonEncode({
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
        setState(() {
          error = data["message"] ?? "OTP sai";
        });
      }
    } catch (e) {
      setState(() {
        error = "Không kết nối được backend";
      });
    }

    if (mounted) {
      setState(() {
        loading = false;
      });
    }
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
            style: const TextStyle(
              fontSize: 30,
              color: Colors.teal,
              fontWeight: FontWeight.bold,
              letterSpacing: 4,
            ),
          ),
          const SizedBox(height: 16),
          TextField(
            controller: otpCtrl,
            keyboardType: TextInputType.number,
            decoration: const InputDecoration(
              labelText: "Nhập lại OTP",
              border: OutlineInputBorder(),
            ),
          ),
          if (error.isNotEmpty)
            Padding(
              padding: const EdgeInsets.only(top: 10),
              child: Text(
                error,
                style: const TextStyle(color: Colors.red),
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
          onPressed: loading ? null : verifyOtp,
          child: loading
              ? const SizedBox(
                  width: 18,
                  height: 18,
                  child: CircularProgressIndicator(strokeWidth: 2),
                )
              : const Text("Xác nhận"),
        ),
      ],
    );
  }
}
class ChangePasswordPage extends StatefulWidget {
  final String token;

  const ChangePasswordPage({
    super.key,
    required this.token,
  });

  @override
  State<ChangePasswordPage> createState() => _ChangePasswordPageState();
}

class _ChangePasswordPageState extends State<ChangePasswordPage> {
  final oldPasswordController = TextEditingController();
  final newPasswordController = TextEditingController();
  final confirmPasswordController = TextEditingController();

  bool loading = false;

  Map<String, String> get authHeaders => {
        "Content-Type": "application/json",
        "Authorization": "Bearer ${widget.token}",
      };

  @override
  void dispose() {
    oldPasswordController.dispose();
    newPasswordController.dispose();
    confirmPasswordController.dispose();
    super.dispose();
  }

  Future<void> changePassword() async {
    final oldPassword = oldPasswordController.text.trim();
    final newPassword = newPasswordController.text.trim();
    final confirmPassword = confirmPasswordController.text.trim();

    if (oldPassword.isEmpty || newPassword.isEmpty || confirmPassword.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text("Vui lòng nhập đầy đủ thông tin"),
        ),
      );
      return;
    }

    if (newPassword.length < 6) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text("Mật khẩu mới phải từ 6 ký tự"),
        ),
      );
      return;
    }

    if (newPassword != confirmPassword) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text("Mật khẩu xác nhận không khớp"),
        ),
      );
      return;
    }

    setState(() {
      loading = true;
    });

    try {
      final res = await http.patch(
        Uri.parse("$baseUrl/api/auth/change-password"),
        headers: authHeaders,
        body: jsonEncode({
          "old_password": oldPassword,
          "new_password": newPassword,
        }),
      );

      final data = jsonDecode(res.body);

      if (!mounted) return;

      if (res.statusCode == 200 && data["success"] == true) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(
              data["message"] ?? "Đổi mật khẩu thành công",
            ),
          ),
        );

        Navigator.pop(context);
      } else {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(
              data["message"] ?? "Đổi mật khẩu thất bại",
            ),
          ),
        );
      }
    } catch (e) {
      if (!mounted) return;

      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text("Lỗi đổi mật khẩu: $e"),
        ),
      );
    } finally {
      if (mounted) {
        setState(() {
          loading = false;
        });
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFFF3FAF9),
      appBar: AppBar(
        title: const Text("Đổi mật khẩu"),
        backgroundColor: const Color(0xFFE0F2F1),
        foregroundColor: Colors.teal,
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(20),
        child: Card(
          elevation: 2,
          child: Padding(
            padding: const EdgeInsets.all(20),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                const Icon(
                  Icons.lock_reset,
                  size: 64,
                  color: Colors.teal,
                ),

                const SizedBox(height: 16),

                const Text(
                  "Thay đổi mật khẩu",
                  textAlign: TextAlign.center,
                  style: TextStyle(
                    fontSize: 22,
                    fontWeight: FontWeight.bold,
                  ),
                ),

                const SizedBox(height: 24),

                TextField(
                  controller: oldPasswordController,
                  obscureText: true,
                  decoration: const InputDecoration(
                    labelText: "Mật khẩu cũ",
                    prefixIcon: Icon(Icons.lock),
                    border: OutlineInputBorder(),
                  ),
                ),

                const SizedBox(height: 16),

                TextField(
                  controller: newPasswordController,
                  obscureText: true,
                  decoration: const InputDecoration(
                    labelText: "Mật khẩu mới",
                    prefixIcon: Icon(Icons.lock_reset),
                    border: OutlineInputBorder(),
                  ),
                ),

                const SizedBox(height: 16),

                TextField(
                  controller: confirmPasswordController,
                  obscureText: true,
                  decoration: const InputDecoration(
                    labelText: "Nhập lại mật khẩu mới",
                    prefixIcon: Icon(Icons.lock_outline),
                    border: OutlineInputBorder(),
                  ),
                ),

                const SizedBox(height: 24),

                FilledButton.icon(
                  onPressed: loading ? null : changePassword,
                  icon: const Icon(Icons.check_circle),
                  label: loading
                      ? const Text("Đang xử lý...")
                      : const Text("Đổi mật khẩu"),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}