#pragma once

#include "knapsack/knapsack.hpp"

namespace knapsack::detail {

// Exercises the complete stage-1 -> stage-2 integration with several prescribed
// layers and the universally safe 2*w_max^2 radius. No unproved small constants
// are injected into the production proximity bounds.
Solution solve_partitioned_reference(std::span<const Item> items, std::uint64_t capacity,
                                      std::size_t layers, Statistics& statistics);

} // namespace knapsack::detail
