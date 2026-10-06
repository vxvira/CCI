#include <iostream>
#include <vector>
#include <numeric>

#include "../vendor/azbacktest/azbacktest.h"
#include "tooling/cci.h"
#include "tooling/atr.h"

int main() {
    std::cout << "Running..." << std::endl;

    loadConfig();

    std::vector<double> prices;
    std::vector<double> highs, lows; 
    MarketData md(kCSVMapping.path);
    Handling handler(prices, 0.25, 0.50);

    const int timeframe = 300;   
    const int batchSize = 500;  

    int tp = 170;
    int sl = -70;

    const int lengths[] = {5,14,25,40};

    int barsPassedSinceReset;
    int barsInDay = 288; // 288 5 minute periods in a day

    double dailyDrawdown;
    double maxDrawdown = 140;
    std::vector<double> lossesInDay;

    int bar = 0;
    for (;;) {
        DataWindow window = handler.requestDataWindow(md, batchSize, timeframe);
        if (window.prices.empty()) break;

        for (std::size_t b = 0; b < window.prices.size(); b++, bar++) {
            prices.push_back(window.prices[b]);
            highs.push_back(window.highs[b]);
            lows.push_back(window.lows[b]);
            handler.tick(window.tsRecv[b]);

            // get drawdown of day
            barsPassedSinceReset += 1;
            if (barsPassedSinceReset >= barsInDay) { 
                dailyDrawdown = std::accumulate(lossesInDay.begin(), lossesInDay.end(), 0.0);
                barsPassedSinceReset = 0; 
                lossesInDay = {};
            }

            if (prices.size() < lengths[3]) continue; // not enough data
            if (dailyDrawdown >= maxDrawdown) continue; // breached daily drawdown, no new trades

            double cci_average = cci_avg(prices, lengths).back();

            if (atr(highs, lows, prices, 14).back() > 2.5) {
                if (cci_average > 75) handler.openLong(bar);
                if (cci_average < -150) handler.openShort(bar); 
            }

            if (handler.openTrade) {
                if (handler.openTrade->td.profit > tp / 2) sl = 0; // b/e halfway to tp

                if (handler.openTrade->td.profit > tp) handler.closeTrade();
                else if (handler.openTrade->td.profit < sl) handler.closeTrade();
            }

            if (trades.size() > 0 && trades.back().profit < 0) lossesInDay.push_back(trades.back().profit);
        }
    }
    handler.closeAll();

    // monte carlo (daily bucketed)
    const int mcSims = 60;
    auto mcPaths  = returnMonteCarlo(mcSims, 5, 86400);
    auto pctPaths = returnPercentilePaths(mcPaths, {5, 50, 95});
    auto profit   = returnCumProfitBucketed(86400);

    std::vector<std::vector<double>> mainPaths;
    mainPaths.push_back(profit);
    for (auto& p : pctPaths) mainPaths.push_back(std::move(p));

    addLine("mc cloud", mcPaths, {}, RGBA{0.4f, 0.4f, 0.4f, 0.3f});
    addLine("equity + percentiles", mainPaths,
        {"actual", "p5", "p50", "p95"}, RGBA{0.5f, 0.8f, 0.5f, 1.0f});

    addStat("WR", returnWinrate());
    addStat("Total profit", returnCumProfit());
    addStat("Average PnL / trade", returnAvgPnl());

    panelManagement::newPanel("equity");
    const char* names[] = {"actual", "p5", "p50", "p95"};
    for (std::size_t i = 0; i < mainPaths.size(); i++)
        newLineSeries("equity", names[i], mainPaths[i]);

    widgetManagement::newWindow("stats");
    widgetManagement::Widget stats{widgetManagement::StatisticExplorer, "Statistic Explorer"};
    for (int i = 0; i < (int)statPool::pool.size(); i++) stats.selectedStatIdxs.push_back(i);
    widgetManagement::findWindow("stats")->children.push_back(stats);

    setTiling(true);
    setTileGrid(4, 1);
    tileWindow("panel_equity", 0, 0, 3, 1);
    tileWindow("widget_stats", 3, 0);

    showConsole("Console", skins::gilded);
}
