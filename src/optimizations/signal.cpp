#include <algorithm>
#include <cstdio>
#include <vector>

#include "../../vendor/azbacktest/azbacktest.h"
#include "../tooling/cci.h"
#include "../tooling/atr.h"
#include "../tooling/bars.h"
#include "../tooling/system.h"

// stage 1: grid search the CCI lengths and both thresholds together at main's
// exits and filters, over the first 75% of the data
int main() {
    loadConfig();

    Bars bars = loadBars(300);
    Indicators ind(bars);
    const Rules base; // main as it stood

    struct Result { Rules r; Summary s; };
    std::vector<Result> results;

    for (int a : {3, 5, 8, 10})
    for (int b : {10, 14, 20})
    for (int c : {20, 25, 30})
    for (int d : {40, 50, 60, 80}) {
        if (!(a < b && b < c && c < d)) continue;
        for (double up = 50; up <= 250; up += 25)
        for (double down = 50; down <= 250; down += 25) {
            Rules r = base;
            r.lengths[0] = a; r.lengths[1] = b; r.lengths[2] = c; r.lengths[3] = d;
            r.up = up; r.down = down;
            results.push_back({r, summarize(runSystem(ind, r, 0, bars.split), bars, 0, bars.split)});
        }
    }

    std::sort(results.begin(), results.end(), [](auto& a, auto& b) { return a.s.pnl > b.s.pnl; });
    std::printf("%-12s %5s %5s %10s %6s %6s %5s %9s %6s\n",
                "lengths", "up", "down", "pnl $", "trades", "win%", "pf", "maxDD $", "sharpe");
    for (std::size_t i = 0; i < 15 && i < results.size(); i++) {
        auto& [r, s] = results[i];
        char ls[32];
        std::snprintf(ls, sizeof ls, "%d,%d,%d,%d", r.lengths[0], r.lengths[1], r.lengths[2], r.lengths[3]);
        std::printf("%-12s %5.0f %5.0f %10.2f %6d %5.1f%% %5.2f %9.2f %6.2f\n", ls, r.up, r.down,
                    s.pnl, s.trades, 100 * s.winRate, s.pf, s.maxDD, s.sharpe);
    }
}
