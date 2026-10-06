#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

// everything one version of the strategy decides with, distances in points
struct Rules {
    int lengths[4] = {5, 14, 25, 40}; // CCIs averaged into the signal
    double up = 75, down = 150;       // long above up, short below -down
    double tp = 170, sl = 70;
    double be = 0.5;                  // stop to breakeven once this far to tp, 0 never
    int maxBars = 0;                  // close after this many bars, 0 never
    double atrMin = 2.5;              // ATR(14) floor for new entries
    double maxDailyLoss = 140;        // no new entries after losing this much in a UTC day
    int emaLen = 0;                   // longs only above the EMA, shorts only below, 0 off
    int startHour = 0, endHour = 24;  // UTC hours new entries are allowed in
    int flowLen = 0;                  // orderflow window, 0 off
    double flowMin = 0;               // (buys - sells) / volume over flowLen, signed with the trade
    bool longs = true, shorts = true;
};

struct Fill { double pts; long long ts; int dir; std::size_t entry, exit; }; // pts net of costs

// indicator series over the whole file, built once per length and shared by every grid cell
struct Indicators {
    const Bars& b;
    std::map<int, std::vector<double>> ccis, emas, flows;
    std::vector<double> atr14;
    explicit Indicators(const Bars& bars) : b(bars), atr14(atr(bars.highs, bars.lows, bars.closes, 14)) {}

    const std::vector<double>& cciOf(int n) {
        auto it = ccis.find(n);
        return it != ccis.end() ? it->second : ccis[n] = cci(b.closes, n);
    }
    const std::vector<double>& emaOf(int n) {
        auto it = emas.find(n);
        if (it != emas.end()) return it->second;
        std::vector<double> e(b.size());
        const double k = 2.0 / (n + 1);
        for (std::size_t i = 0; i < b.size(); i++) e[i] = i ? e[i - 1] + k * (b.closes[i] - e[i - 1]) : b.closes[0];
        return emas[n] = e;
    }
    const std::vector<double>& flowOf(int n) {
        auto it = flows.find(n);
        if (it != flows.end()) return it->second;
        std::vector<double> f(b.size(), 0.0);
        double d = 0, v = 0;
        for (std::size_t i = 0; i < b.size(); i++) {
            d += b.buys[i] - b.sells[i]; v += b.volumes[i];
            if (i >= (std::size_t)n) { d -= b.buys[i - n] - b.sells[i - n]; v -= b.volumes[i - n]; }
            f[i] = v > 0 ? d / v : 0.0;
        }
        return flows[n] = f;
    }
};

// replay bars [from, to) under the rules, one trade at a time
// entries fill at the signal bar's close, exits are checked from the next bar on
// with the stop first when both sides are inside one bar, gaps fill at the open
inline std::vector<Fill> runSystem(Indicators& ind, const Rules& r, std::size_t from, std::size_t to) {
    const Bars& b = ind.b;
    const double cost = kCSVMapping.commision + kCSVMapping.spread + kCSVMapping.timingCost;

    std::vector<double> signal(b.size(), 0.0);
    for (int n : r.lengths) {
        const auto& s = ind.cciOf(n);
        for (std::size_t i = from; i < to; i++) signal[i] += s[i] / 4; // NaN stays NaN
    }
    const auto* ema = r.emaLen ? &ind.emaOf(r.emaLen) : nullptr;
    const auto* flow = r.flowLen ? &ind.flowOf(r.flowLen) : nullptr;

    std::vector<Fill> fills;
    int dir = 0; // 1 long, -1 short, 0 flat
    double entry = 0, stopDist = r.sl;
    std::size_t entryIdx = 0;
    long long day = -1;
    double dayLoss = 0; // losing trades closed today, pts (<= 0)

    for (std::size_t i = from; i < to; i++) {
        if (b.ts[i] / 86400 != day) { day = b.ts[i] / 86400; dayLoss = 0; }

        if (dir) {
            const double slPx = entry - dir * stopDist, tpPx = entry + dir * r.tp;
            const double adverse = dir > 0 ? b.lows[i] : b.highs[i];
            const double favorable = dir > 0 ? b.highs[i] : b.lows[i];
            double exit = 0;
            if (dir * (adverse - slPx) <= 0)        exit = dir * b.opens[i] < dir * slPx ? b.opens[i] : slPx;
            else if (dir * (favorable - tpPx) >= 0) exit = dir * b.opens[i] > dir * tpPx ? b.opens[i] : tpPx;
            else if (r.maxBars && i - entryIdx >= (std::size_t)r.maxBars) exit = b.closes[i];
            else {
                if (r.be > 0 && dir * (favorable - entry) >= r.tp * r.be) stopDist = std::min(stopDist, 0.0); // from the next bar
                continue;
            }

            const double pts = dir * (exit - entry);
            fills.push_back({pts - cost, b.ts[i], dir, entryIdx, i});
            if (pts < 0) dayLoss += pts;
            dir = 0;
            continue;
        }

        if (-dayLoss >= r.maxDailyLoss || !(ind.atr14[i] > r.atrMin)) continue;
        const int hour = (int)(b.ts[i] % 86400 / 3600);
        if (hour < r.startHour || hour >= r.endHour) continue;

        int want = 0;
        if (r.longs && signal[i] > r.up)             want = 1;
        else if (r.shorts && signal[i] < -r.down)    want = -1;
        if (!want) continue;
        if (ema && want * (b.closes[i] - (*ema)[i]) <= 0) continue;
        if (flow && want * (*flow)[i] < r.flowMin) continue;

        dir = want;
        entry = b.closes[i];
        entryIdx = i;
        stopDist = r.sl;
    }
    if (dir) fills.push_back({dir * (b.closes[to - 1] - entry) - cost, b.ts[to - 1], dir, entryIdx, to - 1});
    return fills;
}

struct Summary {
    double pnl = 0, pf = 0, maxDD = 0, sharpe = 0, avg = 0, avgWin = 0, avgLoss = 0, winRate = 0, longPnl = 0, shortPnl = 0;
    int trades = 0, longs = 0, days = 0;
};

// dollar stats for a run, sharpe from daily P&L over every day the range has bars
inline Summary summarize(const std::vector<Fill>& fills, const Bars& b, std::size_t from, std::size_t to,
                         double tickSize = 0.25, double tickValue = 0.50) {
    const double usd = tickValue / tickSize;
    Summary s;
    double gw = 0, gl = 0, eq = 0, peak = 0;
    int wins = 0;
    for (auto& f : fills) {
        const double d = f.pts * usd;
        s.pnl += d; s.trades++;
        (f.dir > 0 ? s.longPnl : s.shortPnl) += d;
        s.longs += f.dir > 0;
        if (d > 0) { gw += d; wins++; } else gl -= d;
        eq += d; peak = std::max(peak, eq); s.maxDD = std::max(s.maxDD, peak - eq);
    }
    s.pf = gl > 0 ? gw / gl : 0;
    s.winRate = s.trades ? (double)wins / s.trades : 0;
    s.avg = s.trades ? s.pnl / s.trades : 0;
    s.avgWin = wins ? gw / wins : 0;
    s.avgLoss = s.trades > wins ? -gl / (s.trades - wins) : 0;

    std::map<long long, double> daily;
    for (std::size_t i = from; i < to; i++) daily[b.ts[i] / 86400];
    for (auto& f : fills) daily[f.ts / 86400] += f.pts * usd;
    s.days = (int)daily.size();
    double m = 0, v = 0;
    for (auto& [d, p] : daily) m += p;
    m /= std::max(1, s.days);
    for (auto& [d, p] : daily) v += (p - m) * (p - m);
    v /= std::max(1, s.days - 1);
    s.sharpe = v > 0 ? m / std::sqrt(v) * std::sqrt(252.0) : 0;
    return s;
}
