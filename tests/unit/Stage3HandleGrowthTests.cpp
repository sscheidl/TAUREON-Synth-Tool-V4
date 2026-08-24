#include "HandleGrowth.hpp"
#include "TestSupport.hpp"

#include <cstdint>
#include <vector>

using taureon::test::analyze_handle_growth;

namespace {

void handle_growth_tests() {
    std::vector<std::uint32_t> flat(100, 200);
    TAUREON_REQUIRE(!analyze_handle_growth(flat, 50).sustained_growth);

    std::vector<std::uint32_t> reversible_jitter;
    reversible_jitter.insert(reversible_jitter.end(), 40, 209);
    reversible_jitter.insert(reversible_jitter.end(), 30, 211);
    reversible_jitter.insert(reversible_jitter.end(), 20, 207);
    reversible_jitter.insert(reversible_jitter.end(), 10, 209);
    const auto jitter = analyze_handle_growth(reversible_jitter, 50);
    TAUREON_REQUIRE(jitter.steady_maximum - jitter.steady_minimum == 4);
    TAUREON_REQUIRE(!jitter.sustained_growth);

    std::vector<std::uint32_t> warmup_then_plateau;
    for (std::uint32_t index = 0; index < 50; ++index) {
        warmup_then_plateau.push_back(100 + index / 10);
    }
    warmup_then_plateau.insert(warmup_then_plateau.end(), 50, 104);
    TAUREON_REQUIRE(!analyze_handle_growth(warmup_then_plateau, 50).sustained_growth);

    std::vector<std::uint32_t> monotonic_leak;
    for (std::uint32_t index = 0; index < 100; ++index) {
        monotonic_leak.push_back(100 + index);
    }
    const auto monotonic = analyze_handle_growth(monotonic_leak, 50);
    TAUREON_REQUIRE(monotonic.steady_slope > 0.0L);
    TAUREON_REQUIRE(monotonic.new_steady_high);
    TAUREON_REQUIRE(monotonic.trailing_median > monotonic.leading_median);
    TAUREON_REQUIRE(monotonic.sustained_growth);

    std::vector<std::uint32_t> slow_step_leak;
    for (std::uint32_t index = 0; index < 500; ++index) {
        slow_step_leak.push_back(200 + index / 100);
    }
    TAUREON_REQUIRE(analyze_handle_growth(slow_step_leak, 250).sustained_growth);

    std::vector<std::uint32_t> late_transient(100, 200);
    late_transient[75] = 202;
    TAUREON_REQUIRE(!analyze_handle_growth(late_transient, 50).sustained_growth);

    // A short diagnostic series has no separate warm-up reference. The new-peak signal is
    // explicitly not applicable even when the short series itself rises monotonically.
    const std::vector<std::uint32_t> short_series{200, 201, 202, 203, 204,
                                                  205, 206, 207, 208, 209};
    const auto short_analysis = analyze_handle_growth(short_series, 0);
    TAUREON_REQUIRE(short_analysis.steady_slope > 0.0L);
    TAUREON_REQUIRE(!short_analysis.new_steady_high);
    TAUREON_REQUIRE(!short_analysis.sustained_growth);
}

} // namespace

int main() { return taureon::test::run([] { handle_growth_tests(); }); }
