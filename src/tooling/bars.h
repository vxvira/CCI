#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// every bar of the data file at one timeframe, with the orderflow split per bar
// split is the first bar past 75% of the file, the same cut the optimizers train on
struct Bars {
    std::vector<double> opens, highs, lows, closes, volumes, buys, sells;
    std::vector<long long> ts; // ts_recv of the closing tick, epoch seconds
    std::size_t split = 0;
    std::size_t size() const { return closes.size(); }
};

// reading the csv takes minutes, so the bars are cached in build/ next to the binaries
// and only re-read when the data file changes size
inline Bars loadBars(int timeframe) {
    Bars b;
    const auto fileSize = std::filesystem::file_size(kCSVMapping.path);
    const std::string cache = "build/bars_" + std::to_string(timeframe) + ".bin";

    auto io = [&](auto& f, auto rw) {
        std::uintmax_t n = b.size(), size = fileSize;
        rw(f, &size, sizeof size); rw(f, &n, sizeof n); rw(f, &b.split, sizeof b.split);
        if (size != fileSize) return false;
        for (auto* v : {&b.opens, &b.highs, &b.lows, &b.closes, &b.volumes, &b.buys, &b.sells}) {
            v->resize(n);
            rw(f, v->data(), n * sizeof(double));
        }
        b.ts.resize(n);
        rw(f, b.ts.data(), n * sizeof(long long));
        return (bool)f;
    };

    if (std::ifstream in{cache, std::ios::binary}) {
        if (io(in, [](auto& f, void* p, std::size_t n) { f.read((char*)p, n); })) {
            std::printf("%zu bars (cached), %zu train\n", b.size(), b.split);
            return b;
        }
        b = {};
    }

    std::vector<double> prices;
    MarketData md(kCSVMapping.path);
    Handling handler(prices, 0.25, 0.50);
    const auto stop = (std::uintmax_t)(fileSize * 0.75);
    for (;;) {
        DataWindow w = handler.requestDataWindow(md, 500, timeframe);
        if (w.prices.empty()) break;
        b.opens.insert(b.opens.end(), w.opens.begin(), w.opens.end());
        b.highs.insert(b.highs.end(), w.highs.begin(), w.highs.end());
        b.lows.insert(b.lows.end(), w.lows.begin(), w.lows.end());
        b.closes.insert(b.closes.end(), w.prices.begin(), w.prices.end());
        b.volumes.insert(b.volumes.end(), w.volumes.begin(), w.volumes.end());
        b.buys.insert(b.buys.end(), w.executedBuys.begin(), w.executedBuys.end());
        b.sells.insert(b.sells.end(), w.executedSells.begin(), w.executedSells.end());
        b.ts.insert(b.ts.end(), w.tsRecv.begin(), w.tsRecv.end());
        if (!b.split && md.byteOffset() >= stop) b.split = b.size();
        std::printf("\rloading %3.0f%%", 100.0 * md.byteOffset() / fileSize);
        std::fflush(stdout);
    }
    if (!b.split) b.split = b.size();
    std::printf("\n%zu bars, %zu train\n", b.size(), b.split);

    std::filesystem::create_directories("build");
    std::ofstream out{cache, std::ios::binary};
    io(out, [](auto& f, void* p, std::size_t n) { f.write((const char*)p, n); });
    return b;
}
