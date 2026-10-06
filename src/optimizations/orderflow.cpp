#include <algorithm>
#include <cstdio>
#include <vector>

#include "../../vendor/azbacktest/azbacktest.h"
#include "../tooling/cci.h"
#include "../tooling/atr.h"
#include "../tooling/bars.h"
#include "../tooling/system.h"

// stage 4: grid search orderflow on top of stage 3, entries need the window's
// (buys - sells) / volume, signed with the trade, inside [min, max] so both
// following and fading the flow get tried, and trades can close once the flow
// turns against them, over the first 75% of the data
int main() {
    loadConfig();

    Bars bars = loadBars(300);
    Indicators ind(bars);
    Rules base; // stage 3, the filters didn't beat noise so they stay off
    base.lengths[0] = 8; base.lengths[1] = 20; base.lengths[2] = 30; base.lengths[3] = 60;
    base.up = 50; base.down = 200;
    base.tp = 180; base.sl = 50; base.be = 0.75;

    struct Result { Rules r; Summary s; };
    std::vector<Result> results;

    for (int flowLen : {0, 1, 3, 6, 12, 24, 48})
    for (double flowMin : {-1.0, -0.04, -0.02, 0.0, 0.01, 0.02, 0.04})
    for (double flowMax : {1.0, 0.04, 0.02, 0.0, -0.02})
    for (int flowExitLen : {0, 1, 3, 6, 12, 24})
    for (double flowExit : {0.0, 0.02, 0.04, 0.06, 0.1}) {
        if (flowMin >= flowMax || (!flowLen && (flowMin > -1 || flowMax < 1))) continue; // gate off once
        if (!flowExitLen && flowExit > 0) continue;
        Rules r = base;
        r.flowLen = flowLen; r.flowMin = flowMin; r.flowMax = flowMax;
        r.flowExitLen = flowExitLen; r.flowExit = flowExit;
        results.push_back({r, summarize(runSystem(ind, r, 0, bars.split), bars, 0, bars.split)});
    }

    std::sort(results.begin(), results.end(), [](auto& a, auto& b) { return a.s.pnl > b.s.pnl; });
    std::printf("%5s %6s %6s %7s %6s %10s %6s %6s %5s %9s %6s\n", "len", "min", "max",
                "exitLen", "exit", "pnl $", "trades", "win%", "pf", "maxDD $", "sharpe");
    for (std::size_t i = 0; i < 15 && i < results.size(); i++) {
        auto& [r, s] = results[i];
        std::printf("%5d %6.2f %6.2f %7d %6.2f %10.2f %6d %5.1f%% %5.2f %9.2f %6.2f\n", r.flowLen, r.flowMin,
                    r.flowMax, r.flowExitLen, r.flowExit, s.pnl, s.trades, 100 * s.winRate, s.pf, s.maxDD, s.sharpe);
    }
}
