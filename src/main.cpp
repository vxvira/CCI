#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "../vendor/azbacktest/azbacktest.h"
#include "tooling/cci.h"
#include "tooling/atr.h"
#include "tooling/bars.h"
#include "tooling/system.h"

// cumulative $ at the end of every day the range has bars
std::vector<double> dailyEquity(const std::vector<Fill>& fills, const Bars& b, std::size_t from, std::size_t to) {
    std::map<long long, double> daily;
    for (std::size_t i = from; i < to; i++) daily[b.ts[i] / 86400];
    for (auto& f : fills) daily[f.ts / 86400] += f.pts * 0.50 / 0.25;
    std::vector<double> curve;
    double eq = 0;
    for (auto& [d, p] : daily) curve.push_back(eq += p);
    return curve;
}

int main() {
    std::cout << "Running..." << std::endl;

    loadConfig();

    Bars bars = loadBars(300);
    Indicators ind(bars);

    // main as it stood before the optimizer stages
    const Rules baseline;

    // frozen from the train-only stages in src/optimizations, the last 25% was never looked at
    Rules optimized;
    optimized.lengths[0] = 8; optimized.lengths[1] = 20; optimized.lengths[2] = 30; optimized.lengths[3] = 60;
    optimized.up = 50; optimized.down = 200;                       // stage 1, signal.cpp
    optimized.tp = 180; optimized.sl = 50; optimized.be = 0.75;    // stage 2, exits.cpp
                                                                   // stage 3, filters.cpp, nothing kept
    optimized.flowLen = 6; optimized.flowMin = -0.04;              // stage 4, orderflow.cpp
                                                                   // stage 5, risk.cpp, nothing kept

    struct System { const char* name; Rules r; };
    struct Segment { const char* name; std::size_t from, to; };
    const System systems[] = {{"baseline", baseline}, {"optimized", optimized}};
    const Segment segments[] = {{"train", 0, bars.split}, {"test", bars.split, bars.size()}};

    std::printf("\n%-10s %-6s %10s %6s %6s %5s %9s %6s %8s %8s %8s %10s %10s\n", "system", "set", "pnl $",
                "trades", "win%", "pf", "maxDD $", "sharpe", "avg $", "avgWin $", "avgLoss $", "long $", "short $");

    for (auto& seg : segments) {
        panelManagement::newPanel(std::string("equity ") + seg.name);
        for (auto& sys : systems) {
            const auto fills = runSystem(ind, sys.r, seg.from, seg.to);
            const Summary s = summarize(fills, bars, seg.from, seg.to);
            const std::string tag = std::string(sys.name) + " " + seg.name + " ";

            std::printf("%-10s %-6s %10.2f %6d %5.1f%% %5.2f %9.2f %6.2f %8.2f %8.2f %8.2f %10.2f %10.2f\n",
                        sys.name, seg.name, s.pnl, s.trades, 100 * s.winRate, s.pf, s.maxDD, s.sharpe,
                        s.avg, s.avgWin, s.avgLoss, s.longPnl, s.shortPnl);

            addStat(tag + "PnL $", s.pnl);
            addStat(tag + "trades", s.trades);
            addStat(tag + "WR", s.winRate);
            addStat(tag + "profit factor", s.pf);
            addStat(tag + "max drawdown $", s.maxDD);
            addStat(tag + "sharpe (daily, ann.)", s.sharpe);
            addStat(tag + "avg PnL / trade $", s.avg);
            addStat(tag + "avg win $", s.avgWin);
            addStat(tag + "avg loss $", s.avgLoss);
            addStat(tag + "long PnL $", s.longPnl);
            addStat(tag + "short PnL $", s.shortPnl);
            addStat(tag + "trades / day", s.days ? (double)s.trades / s.days : 0);

            const auto curve = dailyEquity(fills, bars, seg.from, seg.to);
            addLine(tag + "equity", curve);
            newLineSeries(std::string("equity ") + seg.name, sys.name, curve);

            if (&sys != &systems[1] || &seg != &segments[1]) continue;

            // monte carlo (daily bucketed) of the optimized system out of sample, in $
            trades.clear();
            for (auto& f : fills) trades.push_back({f.pts * 0.50 / 0.25, f.pts > 0, f.ts});
            const int mcSims = 60;
            auto mcPaths  = returnMonteCarlo(mcSims, 5, 86400);
            auto pctPaths = returnPercentilePaths(mcPaths, {5, 50, 95});
            addLine("mc cloud", mcPaths, {}, RGBA{0.4f, 0.4f, 0.4f, 0.3f});
            const char* names[] = {"mc p5", "mc p50", "mc p95"};
            for (std::size_t i = 0; i < pctPaths.size(); i++)
                newLineSeries("equity test", names[i], pctPaths[i]);
        }
    }

    std::fflush(stdout); // the window loop below never returns

    widgetManagement::newWindow("stats");
    widgetManagement::Widget stats{widgetManagement::StatisticExplorer, "Statistic Explorer"};
    for (int i = 0; i < (int)statPool::pool.size(); i++) stats.selectedStatIdxs.push_back(i);
    widgetManagement::findWindow("stats")->children.push_back(stats);

    setTiling(true);
    setTileGrid(4, 2);
    tileWindow("panel_equity train", 0, 0, 3, 1);
    tileWindow("panel_equity test", 0, 1, 3, 1);
    tileWindow("widget_stats", 3, 0, 1, 2);

    showConsole("Console", skins::gilded);
}
