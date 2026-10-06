#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <vector>

#include "../../vendor/azbacktest/azbacktest.h"
#include "../tooling/cci.h"

// grid search the long/short CCI entry thresholds at main's TP/SL (points)
// over the first 75% of the data
int main() {
    loadConfig();

    const int timeframe = 300;
    const double tickSize = 0.25, tickValue = 0.50;
    const double tp = 170, sl = 70; // stop moves to breakeven once halfway to tp
    int lengths[] = {5, 14, 25, 40};

    // read the bars once, every grid cell just replays them
    std::vector<double> opens, highs, lows, closes;
    MarketData md(kCSVMapping.path);
    Handling handler(closes, tickSize, tickValue);
    const auto stop = (std::uintmax_t)(std::filesystem::file_size(kCSVMapping.path) * 0.75);
    while (md.byteOffset() < stop) {
        DataWindow w = handler.requestDataWindow(md, 500, timeframe);
        if (w.prices.empty()) break;
        opens.insert(opens.end(), w.opens.begin(), w.opens.end());
        highs.insert(highs.end(), w.highs.begin(), w.highs.end());
        lows.insert(lows.end(), w.lows.begin(), w.lows.end());
        closes.insert(closes.end(), w.prices.begin(), w.prices.end());
        std::printf("\rloading %3.0f%%", 100.0 * md.byteOffset() / stop);
        std::fflush(stdout);
    }
    std::printf("\n%zu bars\n", closes.size());

    const auto signal = cci_avg(closes, lengths);
    const double cost = kCSVMapping.commision + kCSVMapping.spread + kCSVMapping.timingCost;

    struct Result { double up, down, pnl; int trades, wins; };
    std::vector<Result> results;

    for (double up = 50; up <= 300; up += 25)
    for (double down = 50; down <= 300; down += 25) {
        Result r{up, down, 0, 0, 0};
        int dir = 0; // 1 long, -1 short, 0 flat
        double entry = 0, stopDist = sl;

        for (std::size_t i = 0; i < closes.size(); i++) {
            if (dir) {
                const double slPx = entry - dir * stopDist, tpPx = entry + dir * tp;
                const double adverse = dir > 0 ? lows[i] : highs[i];
                const double favorable = dir > 0 ? highs[i] : lows[i];
                double exit = 0;
                // stop checked first when both are inside one bar, gaps fill at the open
                if (dir * (adverse - slPx) <= 0)        exit = dir * opens[i] < dir * slPx ? opens[i] : slPx;
                else if (dir * (favorable - tpPx) >= 0) exit = dir * opens[i] > dir * tpPx ? opens[i] : tpPx;
                else {
                    if (dir * (favorable - entry) >= tp / 2) stopDist = 0; // breakeven from the next bar
                    continue;
                }

                const double pts = dir * (exit - entry);
                r.pnl += pts; r.trades++; r.wins += pts > 0;
                dir = 0;
                continue;
            }
            if (signal[i] > up)          dir = 1;
            else if (signal[i] < -down)  dir = -1;
            entry = closes[i];
            stopDist = sl;
        }
        if (dir) { r.pnl += dir * (closes.back() - entry); r.trades++; }

        // same per-trade cost Handling::closeTrade charges, both in points
        r.pnl = (r.pnl - r.trades * cost) * tickValue / tickSize;
        results.push_back(r);
    }

    std::sort(results.begin(), results.end(), [](auto& a, auto& b) { return a.pnl > b.pnl; });
    std::printf("%6s %6s %12s %7s %6s\n", "up", "down", "pnl $", "trades", "win%");
    for (std::size_t i = 0; i < 15 && i < results.size(); i++) {
        auto& r = results[i];
        std::printf("%6.0f %6.0f %12.2f %7d %5.1f%%\n", r.up, r.down, r.pnl, r.trades,
                    r.trades ? 100.0 * r.wins / r.trades : 0.0);
    }
}
