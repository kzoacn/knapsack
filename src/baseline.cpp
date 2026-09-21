#include "knapsack/knapsack.hpp"
#include "numeric.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>
#include <vector>

namespace knapsack {

Solution solve_dp(std::span<const Item> items, std::uint64_t capacity, Limits limits) {
    const auto [total_weight, total_profit] = detail::validate_input(items);
    if (total_weight <= capacity) {
        Solution result{total_profit, total_weight, {}};
        for (std::size_t i = 0; i < items.size(); ++i) result.selected.push_back(i);
        return result;
    }
    if (capacity >= limits.max_states)
        throw std::length_error("capacity DP exceeds max_states");
    const auto states = static_cast<std::size_t>(capacity) + 1;
    const auto missing = std::numeric_limits<std::size_t>::max();
    struct Node { std::size_t item, parent; };
    std::vector<Node> nodes;
    std::vector<std::uint64_t> best(states, 0);
    std::vector<std::size_t> heads(states, missing);
    for (std::size_t i = 0; i < items.size(); ++i) {
        const auto w = items[i].weight;
        if (w > capacity) continue;
        for (std::size_t c = states; c-- > w;) {
            const auto candidate = best[c - w] + items[i].profit;
            if (candidate <= best[c]) continue;
            if (nodes.size() >= limits.max_trace_nodes)
                throw std::length_error("capacity DP exceeds max_trace_nodes");
            nodes.push_back({i, heads[c - w]});
            heads[c] = nodes.size() - 1;
            best[c] = candidate;
        }
    }
    Solution result{best.back(), 0, {}};
    for (auto node = heads.back(); node != missing; node = nodes[node].parent) {
        const auto i = nodes[node].item;
        result.selected.push_back(i);
        result.weight += items[i].weight;
    }
    std::sort(result.selected.begin(), result.selected.end());
    return result;
}

Solution solve_brute_force(std::span<const Item> items, std::uint64_t capacity) {
    detail::validate_input(items);
    if (items.size() > 25) throw std::length_error("exhaustive oracle supports at most 25 items");
    const std::uint64_t end = std::uint64_t{1} << items.size();
    std::uint64_t old_mask = 0, winning_mask = 0, weight = 0, profit = 0;
    Solution result;
    for (std::uint64_t step = 1; step < end; ++step) {
        const auto mask = step ^ (step >> 1);
        const auto changed = mask ^ old_mask;
        const auto i = static_cast<std::size_t>(std::countr_zero(changed));
        if (mask & changed) { weight += items[i].weight; profit += items[i].profit; }
        else { weight -= items[i].weight; profit -= items[i].profit; }
        if (weight <= capacity && profit > result.profit) {
            result.profit = profit; result.weight = weight; winning_mask = mask;
        }
        old_mask = mask;
    }
    for (std::size_t i = 0; i < items.size(); ++i)
        if ((winning_mask >> i) & 1) result.selected.push_back(i);
    return result;
}

bool validate_solution(std::span<const Item> items, std::uint64_t capacity,
                       const Solution& solution) {
    std::vector<bool> used(items.size(), false);
    std::uint64_t weight = 0, profit = 0;
    for (auto i : solution.selected) {
        if (i >= items.size() || used[i]) return false;
        used[i] = true;
        if (items[i].weight > std::numeric_limits<std::uint64_t>::max() - weight ||
            items[i].profit > std::numeric_limits<std::uint64_t>::max() - profit) return false;
        weight += items[i].weight;
        profit += items[i].profit;
    }
    return weight <= capacity && weight == solution.weight && profit == solution.profit;
}

} // namespace knapsack
