#include <algorithm>
#include <cstdio>
#include <vector>

#include "../../vendor/azbacktest/azbacktest.h"
#include "../tooling/cci.h"
#include "../tooling/atr.h"
#include "../tooling/bars.h"
#include "../tooling/system.h"

// stage 2: grid search TP, SL, the breakeven trigger and a time stop on top of
// stage 1's signal, over the first 75% of the data
int main() {
    loadConfig();

    Bars bars = loadBars(300);
    Indicators ind(bars);
    Rules base; // stage 1
    base.lengths[0] = 8; base.lengths[1] = 20; base.lengths[2] = 30; base.lengths[3] = 60;
    base.up = 50; base.down = 200;

    struct Result { Rules r; Summary s; };
    std::vector<Result> results;

    for (double tp = 20; tp <= 240; tp += 20)
    for (double sl = 10; sl <= 120; sl += 10)
    for (double be : {0.0, 0.25, 0.5, 0.75})
    for (int maxBars : {0, 12, 48, 96}) {
        Rules r = base;
        r.tp = tp; r.sl = sl; r.be = be; r.maxBars = maxBars;
        results.push_back({r, summarize(runSystem(ind, r, 0, bars.split), bars, 0, bars.split)});
    }

    std::sort(results.begin(), results.end(), [](auto& a, auto& b) { return a.s.pnl > b.s.pnl; });
    std::printf("%5s %5s %5s %7s %10s %6s %6s %5s %9s %6s\n",
                "tp", "sl", "be", "maxBars", "pnl $", "trades", "win%", "pf", "maxDD $", "sharpe");
    for (std::size_t i = 0; i < 15 && i < results.size(); i++) {
        auto& [r, s] = results[i];
        std::printf("%5.0f %5.0f %5.2f %7d %10.2f %6d %5.1f%% %5.2f %9.2f %6.2f\n", r.tp, r.sl, r.be, r.maxBars,
                    s.pnl, s.trades, 100 * s.winRate, s.pf, s.maxDD, s.sharpe);
    }
}
