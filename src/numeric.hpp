#pragma once

#include "knapsack/knapsack.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace knapsack::detail {

using Score = __int128;
using UnsignedScore = unsigned __int128;
inline constexpr Score score_limit = Score{1} << 124;
inline constexpr Score unreachable = -(Score{1} << 126);

inline std::uint64_t checked_add(std::uint64_t a, std::uint64_t b) {
    if (b > std::numeric_limits<std::uint64_t>::max() - a)
        throw std::overflow_error("input aggregate exceeds uint64_t");
    return a + b;
}

inline Score checked_positive_product(Score a, Score b) {
    if (a < 0 || b < 0 || (b && a > score_limit / b))
        throw std::overflow_error("exact score exceeds supported 124-bit range");
    return a * b;
}

inline std::pair<std::uint64_t, std::uint64_t> validate_input(std::span<const Item> items) {
    std::uint64_t weight = 0, profit = 0;
    for (const auto& item : items) {
        if (item.weight == 0 || item.profit == 0)
            throw std::invalid_argument("weights and profits must be positive integers");
        weight = checked_add(weight, item.weight);
        profit = checked_add(profit, item.profit);
    }
    return {weight, profit};
}

inline std::uint64_t ceil_sqrt(UnsignedScore value) {
    if (value == 0) return 0;
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    if (value > UnsignedScore{maximum} * maximum)
        throw std::overflow_error("integer square root exceeds uint64_t");
    std::uint64_t lo = 1, hi = std::numeric_limits<std::uint64_t>::max();
    while (lo < hi) {
        const auto mid = lo + (hi - lo) / 2;
        if (UnsignedScore{mid} * mid >= value) hi = mid;
        else lo = mid + 1;
    }
    return lo;
}

inline std::string score_string(Score value) {
    if (value == unreachable) return "unreachable";
    const bool negative = value < 0;
    UnsignedScore magnitude = negative ? UnsignedScore{0} - static_cast<UnsignedScore>(value)
                                       : static_cast<UnsignedScore>(value);
    std::string result;
    do {
        result += static_cast<char>('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude);
    if (negative) result += '-';
    std::reverse(result.begin(), result.end());
    return result;
}

} // namespace knapsack::detail
