#pragma once

#include "knapsack/knapsack.hpp"
#include "numeric.hpp"

#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace knapsack::detail {

inline constexpr auto no_node = std::numeric_limits<std::size_t>::max();

struct ConcaveFunction {
    std::uint32_t weight = 0;
    std::vector<Score> prefix{0};
    Score tail_slope = 0; // Concave continuation after the finite prefix.
    // Optional physical multiplicity. When finite, the tail must be a penalty
    // making every over-capacity extension worse than every physical one.
    // solve_paper establishes this using -(2*total_perturbed_profit+1).
    std::size_t max_count = no_node;

    Score operator()(std::size_t count) const {
        if (count < prefix.size()) return prefix[count];
        return prefix.back() + tail_slope * static_cast<Score>(count - prefix.size() + 1);
    }
};

struct ExtensionNode {
    std::size_t parent;
    std::size_t function;
    std::size_t count;
};

struct ExtensionEntry {
    Score profit = unreachable;
    std::size_t origin = 0;
    std::size_t head = no_node;
};

struct ExtensionResult {
    std::vector<ExtensionEntry> entries;
    std::vector<ExtensionNode> nodes;
};

using HintSets = std::vector<std::vector<std::size_t>>;

// singleton[i] names the only permitted function at source i, or no_node.
std::vector<ExtensionEntry> extend_singletons(
    std::span<const ExtensionEntry> input,
    std::span<const std::size_t> singleton,
    std::span<const ConcaveFunction> functions,
    std::vector<ExtensionNode>& nodes, Limits limits, Statistics& statistics);

ExtensionResult extend_hinted(std::span<const Score> input, const HintSets& hints,
                              std::span<const ConcaveFunction> functions,
                              Limits limits, Statistics& statistics);

} // namespace knapsack::detail
