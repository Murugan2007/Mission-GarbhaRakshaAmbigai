import 'package:path/path.dart';
import 'package:sqflite/sqflite.dart';

import '../models/models.dart';

/// Local-first storage: everything stays on the phone.
class Store {
  Database? _db;

  Future<void> open() async {
    final path = join(await getDatabasesPath(), 'garbha_raksha.db');
    _db = await openDatabase(
      path,
      version: 1,
      onCreate: (db, _) async {
        await db.execute(
            'CREATE TABLE summaries(time INTEGER, pat REAL, good INTEGER, flags INTEGER, rr INTEGER)');
        await db.execute(
            'CREATE TABLE cuffs(time INTEGER, sys INTEGER, dia INTEGER, pat REAL)');
        await db.execute('CREATE TABLE settings(k TEXT PRIMARY KEY, v TEXT)');
      },
    );
  }

  Future<void> addSummary(BeatSummary s) async {
    await _db!.insert('summaries', {
      'time': s.time.millisecondsSinceEpoch,
      'pat': s.medianPatMs,
      'good': s.goodBeats,
      'flags': s.flags,
      'rr': s.rrMs,
    });
  }

  Future<List<BeatSummary>> summaries({int limit = 5000}) async {
    final rows = await _db!.query('summaries', orderBy: 'time DESC', limit: limit);
    return rows.reversed
        .map((r) => BeatSummary(
              time: DateTime.fromMillisecondsSinceEpoch(r['time'] as int),
              medianPatMs: (r['pat'] as num).toDouble(),
              goodBeats: r['good'] as int,
              flags: r['flags'] as int,
              rrMs: r['rr'] as int,
            ))
        .toList();
  }

  Future<void> addCuff(CuffReading c) async {
    await _db!.insert('cuffs', {
      'time': c.time.millisecondsSinceEpoch,
      'sys': c.sys,
      'dia': c.dia,
      'pat': c.patMs,
    });
  }

  Future<List<CuffReading>> cuffs() async {
    final rows = await _db!.query('cuffs', orderBy: 'time ASC');
    return rows
        .map((r) => CuffReading(
              time: DateTime.fromMillisecondsSinceEpoch(r['time'] as int),
              sys: r['sys'] as int,
              dia: r['dia'] as int,
              patMs: (r['pat'] as num?)?.toDouble(),
            ))
        .toList();
  }

  Future<String?> setting(String key) async {
    final rows = await _db!.query('settings', where: 'k = ?', whereArgs: [key]);
    return rows.isEmpty ? null : rows.first['v'] as String;
  }

  Future<void> putSetting(String key, String value) async {
    await _db!.insert('settings', {'k': key, 'v': value},
        conflictAlgorithm: ConflictAlgorithm.replace);
  }
}
