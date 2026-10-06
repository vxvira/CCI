#include <vector>
#include <cmath>
#include <limits>
#include <algorithm>

// Wilder's ATR, NaN until the first n bars have passed
std::vector<double> atr(const std::vector<double>& high, const std::vector<double>& low,
                        const std::vector<double>& close, std::size_t n = 14) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> out(close.size(), nan);
    if (n == 0 || close.size() < n) return out;

    auto tr = [&](std::size_t t) {
        if (t == 0) return high[0] - low[0];
        return std::max({high[t] - low[t], std::fabs(high[t] - close[t - 1]), std::fabs(low[t] - close[t - 1])});
    };

    double sum = 0.0;
    for (std::size_t t = 0; t < n; ++t) sum += tr(t);
    out[n - 1] = sum / n;                                   // seed with a plain average

    for (std::size_t t = n; t < close.size(); ++t)
        out[t] = (out[t - 1] * (n - 1) + tr(t)) / n;        // then Wilder smoothing
    return out;
}
