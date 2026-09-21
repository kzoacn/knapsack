#pragma once

#include "knapsack/knapsack.hpp"

#include <charconv>
#include <cstdint>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace knapsack::app {

inline std::uint64_t number(std::string_view token) {
    std::uint64_t value = 0;
    const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (error != std::errc{} || end != token.data() + token.size())
        throw std::invalid_argument("invalid unsigned integer: " + std::string(token));
    return value;
}

inline std::uint64_t read_number(std::istream& input) {
    std::string token;
    if (!(input >> token)) throw std::invalid_argument("incomplete input");
    return number(token);
}

inline std::pair<std::vector<Item>, std::uint64_t> read_instance(std::istream& input) {
    const auto n = read_number(input), capacity = read_number(input);
    if (n > 10'000'000) throw std::length_error("CLI accepts at most 10000000 items");
    std::vector<Item> items;
    items.reserve(static_cast<std::size_t>(n));
    for (std::uint64_t i = 0; i < n; ++i) {
        const auto weight = read_number(input), profit = read_number(input);
        if (weight == 0 || weight > std::numeric_limits<std::uint32_t>::max() || profit == 0)
            throw std::invalid_argument("weight must fit positive uint32_t; profit must fit positive uint64_t");
        items.push_back({static_cast<std::uint32_t>(weight), profit});
    }
    std::string trailing;
    if (input >> trailing) throw std::invalid_argument("unexpected token after the last item");
    return {std::move(items), capacity};
}

inline Solution dispatch(std::string_view algorithm, std::span<const Item> items,
                          std::uint64_t capacity, Limits limits, Statistics& statistics) {
    statistics = {};
    if (algorithm == "paper") return solve_paper(items, capacity, limits, &statistics);
    if (algorithm == "exchange") return solve_exchange(items, capacity, limits, &statistics);
    if (algorithm == "dp") return solve_dp(items, capacity, limits);
    if (algorithm == "brute") return solve_brute_force(items, capacity);
    throw std::invalid_argument("algorithm must be paper, exchange, dp, or brute");
}

inline void write_solution(std::ostream& out, const Solution& result) {
    out << "profit " << result.profit << "\nweight " << result.weight << "\nitems " << result.selected.size();
    for (auto i : result.selected) out << ' ' << i;
    out << '\n';
}

} // namespace knapsack::app
