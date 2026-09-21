#pragma once

#include "hinted.hpp"

#include <cstddef>
#include <vector>

namespace knapsack::detail {

struct IsolatingColoring {
    std::vector<std::size_t> colors;
    std::vector<std::size_t> isolated; // Sets first isolated by this coloring.
    std::size_t color_count = 0;
};

// Lemma 4.10 / Appendix B.5: exact integer pessimistic estimators.
std::vector<IsolatingColoring> isolating_family(const HintSets& sets,
                                               std::size_t universe_size);

// Appendix B.4. r must be a power of two. Each recursive split is checked
// against its integer discrepancy bound before its coloring is accepted.
std::vector<std::size_t> balanced_coloring(const HintSets& sets,
                                          std::size_t universe_size, std::size_t r);

} // namespace knapsack::detail
