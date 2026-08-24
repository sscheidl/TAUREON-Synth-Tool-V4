#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace taureon::test {

struct HandleGrowthAnalysis {
    std::uint32_t steady_minimum{};
    std::uint32_t steady_maximum{};
    std::uint32_t reference_maximum{};
    long double steady_slope{};
    long double leading_median{};
    long double trailing_median{};
    bool new_steady_high{};
    bool sustained_growth{};
};

inline long double median(std::vector<std::uint32_t> values) {
    if (values.empty()) return 0.0L;
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    if (values.size() % 2 != 0) return values[middle];
    return (static_cast<long double>(values[middle - 1]) + values[middle]) / 2.0L;
}

inline HandleGrowthAnalysis analyze_handle_growth(
    const std::span<const std::uint32_t> samples, const std::size_t steady_start_index) {
    HandleGrowthAnalysis result;
    if (samples.empty()) return result;

    const auto start = (std::min)(steady_start_index, samples.size() - 1);
    const auto steady = samples.subspan(start);
    const auto [minimum, maximum] = std::minmax_element(steady.begin(), steady.end());
    result.steady_minimum = *minimum;
    result.steady_maximum = *maximum;

    const auto edge_count = (std::max)(std::size_t{1}, steady.size() / 5);
    const std::vector<std::uint32_t> leading(steady.begin(), steady.begin() + edge_count);
    const std::vector<std::uint32_t> trailing(steady.end() - edge_count, steady.end());
    result.leading_median = median(leading);
    result.trailing_median = median(trailing);

    if (start != 0) {
        result.reference_maximum =
            *std::max_element(samples.begin(), samples.begin() + start);
        result.new_steady_high = result.steady_maximum > result.reference_maximum;
    } else {
        result.reference_maximum = *std::max_element(leading.begin(), leading.end());
        // Without a distinct warm-up segment there is no independent historical envelope.
        // Report the peak criterion as not applicable instead of comparing the series to itself.
        result.new_steady_high = false;
    }

    if (steady.size() > 1) {
        const long double mean_x = static_cast<long double>(steady.size() - 1) / 2.0L;
        long double mean_y{};
        for (const auto value : steady) mean_y += value;
        mean_y /= static_cast<long double>(steady.size());

        long double numerator{};
        long double denominator{};
        for (std::size_t index = 0; index < steady.size(); ++index) {
            const auto x = static_cast<long double>(index) - mean_x;
            numerator += x * (static_cast<long double>(steady[index]) - mean_y);
            denominator += x * x;
        }
        result.steady_slope = denominator == 0.0L ? 0.0L : numerator / denominator;
    }

    result.sustained_growth = result.steady_slope > 0.0L && result.new_steady_high &&
                              result.trailing_median > result.leading_median;
    return result;
}

} // namespace taureon::test
