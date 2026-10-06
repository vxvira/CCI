#include <vector>
#include <cmath>
#include <limits>

std::vector<double> cci(const std::vector<double>& x, std::size_t n = 20) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> out(x.size(), nan);
    if (n == 0 || x.size() < n) return out;

    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i) sum += x[i];

    for (std::size_t t = n - 1; t < x.size(); ++t) {
        if (t >= n) sum += x[t] - x[t - n];         
        const double sma = sum / n;

        double md = 0.0;                              
        for (std::size_t i = t + 1 - n; i <= t; ++i) md += std::fabs(x[i] - sma);
        md /= n;

        out[t] = md > 0.0 ? (x[t] - sma) / (0.015 * md) : 0.0;
    }
    return out;
}

template <typename T, std::size_t N>
std::vector<double> cci_avg(const std::vector<double>& x, const T (&lengths)[N]) {
    std::vector<double> out(x.size(), 0.0);
    for (T n : lengths) {
        auto c = cci(x, static_cast<std::size_t>(n));
        for (std::size_t i = 0; i < x.size(); ++i) out[i] += c[i];   // NaN stays NaN
    }
    for (double& v : out) v /= N;
    return out;
}