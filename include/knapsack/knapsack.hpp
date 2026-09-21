#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace knapsack {

struct Item {
    std::uint32_t weight;
    std::uint64_t profit;
};

struct Solution {
    std::uint64_t profit = 0;
    std::uint64_t weight = 0;
    std::vector<std::size_t> selected; // Zero-based indices in the original input.
};

struct Limits {
    std::size_t max_states = 2'000'001;
    std::size_t max_trace_nodes = 20'000'000;
};

struct Statistics {
    std::uint64_t matrix_queries = 0;
    std::uint64_t candidate_intervals = 0;
    std::uint64_t bucket_visits = 0;
    std::uint64_t singleton_calls = 0;
    std::size_t peak_singleton_sources = 0;
    std::uint64_t colorings = 0;
    std::uint64_t rank_phases = 0;
    std::uint64_t weight_layers = 0;
    std::size_t peak_states = 0;
    std::size_t trace_nodes = 0;
    bool capacity_shortcut = false;
};

// All solvers reject zero weights/profits and overflow of input aggregate sums.
// Resource limits cause an exception, never an approximate answer.
Solution solve_dp(std::span<const Item> items, std::uint64_t capacity,
                  Limits limits = {});
Solution solve_brute_force(std::span<const Item> items, std::uint64_t capacity);
Solution solve_exchange(std::span<const Item> items, std::uint64_t capacity,
                         Limits limits = {}, Statistics* statistics = nullptr);
Solution solve_paper(std::span<const Item> items, std::uint64_t capacity,
                      Limits limits = {}, Statistics* statistics = nullptr);

// Independently checks feasibility, uniqueness, and the reported aggregates.
bool validate_solution(std::span<const Item> items, std::uint64_t capacity,
                       const Solution& solution);

} // namespace knapsack
