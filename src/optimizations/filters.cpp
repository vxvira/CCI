#include <algorithm>
#include <cstdio>
#include <vector>

#include "../../vendor/azbacktest/azbacktest.h"
#include "../tooling/cci.h"
#include "../tooling/atr.h"
#include "../tooling/bars.h"
#include "../tooling/system.h"

// stage 3: grid search the ATR floor, an EMA trend filter and the UTC entry
// window on top of stage 2's exits, over the first 75% of the data
int main() {
    loadConfig();

    Bars bars = loadBars(300);
    Indicators ind(bars);
    Rules base; // stage 2
    base.lengths[0] = 8; base.lengths[1] = 20; base.lengths[2] = 30; base.lengths[3] = 60;
    base.up = 50; base.down = 200;
    base.tp = 180; base.sl = 50; base.be = 0.75;

    struct Result { Rules r; Summary s; };
    std::vector<Result> results;

    for (double atrMin : {2.5, 7.5, 10.0, 15.0, 20.0, 25.0})
    for (int emaLen : {0, 50, 100, 200, 400})
    for (int startHour : {0, 8, 13, 14})
    for (int endHour : {16, 18, 20, 21, 24}) {
        Rules r = base;
        r.atrMin = atrMin; r.emaLen = emaLen; r.startHour = startHour; r.endHour = endHour;
        results.push_back({r, summarize(runSystem(ind, r, 0, bars.split), bars, 0, bars.split)});
    }

    std::sort(results.begin(), results.end(), [](auto& a, auto& b) { return a.s.pnl > b.s.pnl; });
    std::printf("%6s %6s %5s %5s %10s %6s %6s %5s %9s %6s\n",
                "atr>", "ema", "from", "to", "pnl $", "trades", "win%", "pf", "maxDD $", "sharpe");
    for (std::size_t i = 0; i < 15 && i < results.size(); i++) {
        auto& [r, s] = results[i];
        std::printf("%6.1f %6d %5d %5d %10.2f %6d %5.1f%% %5.2f %9.2f %6.2f\n", r.atrMin, r.emaLen, r.startHour,
                    r.endHour, s.pnl, s.trades, 100 * s.winRate, s.pf, s.maxDD, s.sharpe);
    }
}
