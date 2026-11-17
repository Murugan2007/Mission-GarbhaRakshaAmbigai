import 'package:flutter/material.dart';

import 'screens/about_screen.dart';
import 'screens/calibration_screen.dart';
import 'screens/history_screen.dart';
import 'screens/today_screen.dart';
import 'services/app_state.dart';
import 'services/store.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  final state = AppState(Store());
  await state.init();
  runApp(GarbhaRakshaApp(state: state));
}

class GarbhaRakshaApp extends StatelessWidget {
  const GarbhaRakshaApp({super.key, required this.state});
  final AppState state;

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Mission GarbhaRaksha',
      theme: ThemeData(colorSchemeSeed: Colors.teal, useMaterial3: true),
      home: HomeShell(state: state),
    );
  }
}

class HomeShell extends StatefulWidget {
  const HomeShell({super.key, required this.state});
  final AppState state;

  @override
  State<HomeShell> createState() => _HomeShellState();
}

class _HomeShellState extends State<HomeShell> {
  int _tab = 0;

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: widget.state,
      builder: (context, _) {
        final pages = [
          TodayScreen(state: widget.state),
          HistoryScreen(state: widget.state),
          CalibrationScreen(state: widget.state),
          const AboutScreen(),
        ];
        return Scaffold(
          appBar: AppBar(title: const Text('Mission GarbhaRaksha')),
          body: SafeArea(child: pages[_tab]),
          bottomNavigationBar: NavigationBar(
            selectedIndex: _tab,
            onDestinationSelected: (i) => setState(() => _tab = i),
            destinations: const [
              NavigationDestination(icon: Icon(Icons.favorite_border), label: 'Today'),
              NavigationDestination(icon: Icon(Icons.show_chart), label: 'History'),
              NavigationDestination(icon: Icon(Icons.tune), label: 'Calibrate'),
              NavigationDestination(icon: Icon(Icons.info_outline), label: 'About'),
            ],
          ),
        );
      },
    );
  }
}
