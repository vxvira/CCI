#include <algorithm>
#include <cstdio>
#include <vector>

#include "../../vendor/azbacktest/azbacktest.h"
#include "../tooling/cci.h"
#include "../tooling/atr.h"
#include "../tooling/bars.h"
#include "../tooling/system.h"

// stage 5: grid search the daily loss limit and which sides trade on top of
// stage 4, over the first 75% of the data
int main() {
    loadConfig();

    Bars bars = loadBars(300);
    Indicators ind(bars);
    Rules base; // stage 4, the flow exit didn't beat noise so it stays off
    base.lengths[0] = 8; base.lengths[1] = 20; base.lengths[2] = 30; base.lengths[3] = 60;
    base.up = 50; base.down = 200;
    base.tp = 180; base.sl = 50; base.be = 0.75;
    base.flowLen = 6; base.flowMin = -0.04;

    struct Result { Rules r; Summary s; };
    std::vector<Result> results;

    for (double maxDailyLoss : {50.0, 100.0, 140.0, 200.0, 300.0, 1e9})
    for (int sides : {0, 1, 2}) { // both, long only, short only
        Rules r = base;
        r.maxDailyLoss = maxDailyLoss; r.longs = sides != 2; r.shorts = sides != 1;
        results.push_back({r, summarize(runSystem(ind, r, 0, bars.split), bars, 0, bars.split)});
    }

    std::sort(results.begin(), results.end(), [](auto& a, auto& b) { return a.s.pnl > b.s.pnl; });
    std::printf("%8s %6s %10s %10s %10s %6s %6s %5s %9s %6s\n", "dayLoss", "sides",
                "pnl $", "long $", "short $", "trades", "win%", "pf", "maxDD $", "sharpe");
    for (std::size_t i = 0; i < 15 && i < results.size(); i++) {
        auto& [r, s] = results[i];
        std::printf("%8.0f %6s %10.2f %10.2f %10.2f %6d %5.1f%% %5.2f %9.2f %6.2f\n", std::min(r.maxDailyLoss, 99999.0),
                    r.longs && r.shorts ? "both" : r.longs ? "long" : "short", s.pnl, s.longPnl, s.shortPnl,
                    s.trades, 100 * s.winRate, s.pf, s.maxDD, s.sharpe);
    }
}
