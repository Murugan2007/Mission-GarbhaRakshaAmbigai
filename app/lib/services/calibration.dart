/// Per-person calibration: SBP = a + b / PAT, fitted to paired cuff readings.
/// PAT is a trend signal; this line is only as good as the readings behind it
/// and must be refitted regularly.
class CalibrationModel {
  const CalibrationModel({
    required this.a,
    required this.b,
    required this.n,
    required this.r,
    required this.fitSd,
  });

  final double a;
  final double b;
  final int n;

  /// Correlation between 1/PAT and the cuff value.
  final double r;

  /// Standard deviation of the fit residuals, in mmHg.
  final double fitSd;

  double predict(double patMs) => a + b / patMs;

  /// Least-squares fit. Returns null when there are too few points or no spread.
  static CalibrationModel? fit(List<double> patMs, List<double> cuff, {int minPoints = 6}) {
    if (patMs.length != cuff.length || patMs.length < minPoints) return null;
    final n = patMs.length;
    final x = [for (final p in patMs) 1.0 / p];
    final mx = x.reduce((s, v) => s + v) / n;
    final my = cuff.reduce((s, v) => s + v) / n;
    double sxx = 0, sxy = 0, syy = 0;
    for (var i = 0; i < n; i++) {
      sxx += (x[i] - mx) * (x[i] - mx);
      sxy += (x[i] - mx) * (cuff[i] - my);
      syy += (cuff[i] - my) * (cuff[i] - my);
    }
    if (sxx <= 0 || syy <= 0) return null;
    final b = sxy / sxx;
    final a = my - b * mx;
    double ss = 0;
    for (var i = 0; i < n; i++) {
      final e = cuff[i] - (a + b * x[i]);
      ss += e * e;
    }
    final r = sxy / _sqrt(sxx * syy);
    return CalibrationModel(a: a, b: b, n: n, r: r, fitSd: _sqrt(ss / n));
  }

  static double _sqrt(double v) {
    // Newton iterations; avoids a dart:math import in this tiny helper.
    if (v <= 0) return 0;
    double g = v > 1 ? v / 2 : 1;
    for (var i = 0; i < 30; i++) {
      g = 0.5 * (g + v / g);
    }
    return g;
  }
}
