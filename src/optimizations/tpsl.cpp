#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <vector>

#include "../../vendor/azbacktest/azbacktest.h"
#include "../tooling/cci.h"

// grid search TP/SL (points) for main's CCI signal over the first 75% of the data
int main() {
    loadConfig();

    const int timeframe = 300;
    const double tickSize = 0.25, tickValue = 0.50;
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

    struct Result { double tp, sl, pnl; int trades, wins; };

    for (bool longOnly : {false, true}) {
    std::vector<Result> results;

    for (double tp = 10; tp <= 200; tp += 10)
    for (double sl = 10; sl <= 200; sl += 10) {
        Result r{tp, sl, 0, 0, 0};
        int dir = 0; // 1 long, -1 short, 0 flat
        double entry = 0;

        for (std::size_t i = 0; i < closes.size(); i++) {
            if (dir) {
                const double slPx = entry - dir * sl, tpPx = entry + dir * tp;
                const double adverse = dir > 0 ? lows[i] : highs[i];
                const double favorable = dir > 0 ? highs[i] : lows[i];
                double exit = 0;
                // stop checked first when both are inside one bar, gaps fill at the open
                if (dir * (adverse - slPx) <= 0)        exit = dir * opens[i] < dir * slPx ? opens[i] : slPx;
                else if (dir * (favorable - tpPx) >= 0) exit = dir * opens[i] > dir * tpPx ? opens[i] : tpPx;
                else continue;

                const double pts = dir * (exit - entry);
                r.pnl += pts; r.trades++; r.wins += pts > 0;
                dir = 0;
                continue;
            }
            if (signal[i] > 150)       dir = 1;
            else if (signal[i] < -150 && !longOnly) dir = -1;
            entry = closes[i];
        }
        if (dir) { r.pnl += dir * (closes.back() - entry); r.trades++; }

        // same per-trade cost Handling::closeTrade charges, off the dollar P&L
        r.pnl = r.pnl * tickValue / tickSize - r.trades * cost;
        results.push_back(r);
    }

    std::sort(results.begin(), results.end(), [](auto& a, auto& b) { return a.pnl > b.pnl; });
    std::printf("\n%s\n%6s %6s %12s %7s %6s\n", longOnly ? "long only" : "long + short",
                "tp", "sl", "pnl $", "trades", "win%");
    for (std::size_t i = 0; i < 10 && i < results.size(); i++) {
        auto& r = results[i];
        std::printf("%6.0f %6.0f %12.2f %7d %5.1f%%\n", r.tp, r.sl, r.pnl, r.trades,
                    r.trades ? 100.0 * r.wins / r.trades : 0.0);
    }
    }
}
