#include "common.hpp"
#include "hinted.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>

using namespace knapsack::detail;

// Two color classes of an isolated b=2 instance. The first class has one
// common weight; the second has a different weight for every original source.
// This isolates the matrix-search cost caused by propagated source handles.
void probe(std::size_t roots) {
    const auto rows = 16 * roots * roots;
    const Score alpha = static_cast<Score>(roots - 1);
    const Score spacing = static_cast<Score>(rows / roots);
    std::vector<ConcaveFunction> functions(roots + 1);
    functions[0].weight = 1;
    for (std::size_t count = 1; count <= rows; ++count) {
        const Score x = static_cast<Score>(count);
        functions[0].prefix.push_back(-alpha * x * x);
    }
    functions[0].tail_slope = functions[0].prefix.back() - functions[0].prefix[rows - 1];
    for (std::size_t root = 0; root < roots; ++root) {
        auto& q = functions[root + 1];
        q.weight = static_cast<std::uint32_t>(root + 2);
        const Score w = q.weight, z = static_cast<Score>(root);
        const Score linear = 2 * w * z * (alpha - static_cast<Score>(rows));
        const auto maximum = rows / q.weight + 1;
        for (std::size_t count = 1; count <= maximum; ++count) {
            const Score x = static_cast<Score>(count);
            q.prefix.push_back(linear * x - w * w * x * x);
        }
        q.tail_slope = q.prefix.back() - q.prefix[q.prefix.size() - 2];
    }
    std::vector<ExtensionEntry> input(rows);
    std::vector<std::size_t> hints(rows, no_node);
    for (std::size_t root = 0; root < roots; ++root) {
        const Score z = static_cast<Score>(root);
        input[root] = {-alpha * (spacing - 1) * z * z, root, no_node};
        hints[root] = 0;
    }
    const knapsack::Limits limits{rows, 4 * rows};
    std::vector<ExtensionNode> nodes;
    knapsack::Statistics first_stats, second_stats;
    auto first = extend_singletons(input, hints, functions, nodes, limits, first_stats);
    for (std::size_t row = 0; row < rows; row += std::max<std::size_t>(1, rows / 1024)) {
        Score expected = unreachable;
        for (std::size_t root = 0; root < std::min(roots, row + 1); ++root)
            expected = std::max(expected, input[root].profit + functions[0](row - root));
        if (first[row].profit != expected) throw std::logic_error("first-color envelope mismatch");
    }
    for (std::size_t row = 0; row < rows; ++row) hints[row] = first[row].origin + 1;
    const auto second = extend_singletons(first, hints, functions, nodes, limits, second_stats);
    for (std::size_t row = 0; row < rows; ++row) {
        const auto& entry = second[row];
        auto weight = entry.origin;
        auto profit = input[entry.origin].profit;
        for (auto node = entry.head; node != no_node; node = nodes[node].parent) {
            const auto& step = nodes[node];
            weight += step.count * functions[step.function].weight;
            profit += functions[step.function](step.count);
        }
        if (weight != row || profit != entry.profit) throw std::logic_error("composed witness mismatch");
    }
    const double claimed_scale = static_cast<double>(rows) +
        static_cast<double>(roots) * std::log2(static_cast<double>(rows));
    std::cout << rows << ',' << roots << ',' << second_stats.peak_singleton_sources << ','
              << first_stats.matrix_queries << ',' << second_stats.matrix_queries << ','
              << std::fixed << std::setprecision(4)
              << static_cast<double>(second_stats.matrix_queries) / static_cast<double>(rows) << ','
              << static_cast<double>(second_stats.matrix_queries) / claimed_scale << '\n';
}

void graph_probe(std::size_t count) {
    const auto rows = 4 * count * count;
    HintSets pairs;
    for (std::size_t a = 0; a < count; ++a)
        for (auto b = a + 1; b < count; ++b) pairs.push_back({a, b});
    std::mt19937_64 random(230804093);
    std::shuffle(pairs.begin(), pairs.end(), random);
    const auto special = std::find(pairs.begin(), pairs.end(), std::vector<std::size_t>{0, 1});
    std::iter_swap(special, pairs.begin());
    HintSets hints(rows);
    std::vector<Score> input(rows, unreachable);
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        const Score z = static_cast<Score>(i);
        input[i] = -7 * z * z;
        hints[i] = pairs[i];
    }
    std::vector<ConcaveFunction> functions(count);
    for (std::size_t f = 0; f < count; ++f) {
        auto& q = functions[f];
        q.weight = static_cast<std::uint32_t>(count + f);
        q.max_count = 2 * count;
        q.tail_slope = -(Score{1} << 100);
        for (std::size_t copies = 1; copies <= q.max_count; ++copies) {
            const Score weight = static_cast<Score>(q.weight) * static_cast<Score>(copies);
            q.prefix.push_back(-weight * weight);
        }
    }
    knapsack::Statistics stats;
    const auto result = extend_hinted(input, hints, functions, {rows, 32 * rows}, stats);
    for (std::size_t row = 0; row < rows; ++row) {
        const auto& entry = result.entries[row];
        if (entry.profit == unreachable) continue;
        if (entry.origin >= pairs.size()) throw std::logic_error("graph witness starts at an unreachable source");
        auto weight = entry.origin;
        Score profit = input[entry.origin];
        std::size_t used = 0, previous = no_node;
        for (auto node = entry.head; node != no_node; node = result.nodes[node].parent) {
            const auto& step = result.nodes[node];
            if (step.count > functions[step.function].max_count || ++used > 2 || previous == step.function ||
                (step.function != pairs[entry.origin][0] && step.function != pairs[entry.origin][1]))
                throw std::logic_error("graph witness violates its source hint");
            previous = step.function;
            weight += step.count * functions[step.function].weight;
            profit += functions[step.function](step.count);
        }
        if (weight != row || profit != entry.profit) throw std::logic_error("graph witness mismatch");
    }
    if (count <= 8) {
        // Independent bounded DP per ORIGINAL source, tracking whether ANY
        // unrestricted tied maximizer uses a function outside that source hint.
        std::vector<Score> optimum(rows, unreachable);
        std::vector<bool> invalid(rows, false);
        for (std::size_t source = 0; source < pairs.size(); ++source) {
            std::vector<Score> dp(rows, unreachable);
            std::vector<bool> bad(rows, false);
            dp[source] = input[source];
            for (std::size_t f = 0; f < count; ++f) {
                std::vector<Score> next(rows, unreachable);
                std::vector<bool> next_bad(rows, false);
                for (std::size_t i = source; i < rows; ++i) {
                    if (dp[i] == unreachable) continue;
                    for (std::size_t copies = 0; copies <= functions[f].max_count &&
                         copies * functions[f].weight < rows - i; ++copies) {
                        const auto target = i + copies * functions[f].weight;
                        const auto value = dp[i] + functions[f](copies);
                        const bool violates = bad[i] || (copies && f != pairs[source][0] && f != pairs[source][1]);
                        if (value > next[target]) { next[target] = value; next_bad[target] = violates; }
                        else if (value == next[target]) next_bad[target] = next_bad[target] || violates;
                    }
                }
                dp = std::move(next); bad = std::move(next_bad);
            }
            for (std::size_t i = source; i < rows; ++i) {
                if (dp[i] > optimum[i]) { optimum[i] = dp[i]; invalid[i] = bad[i]; }
                else if (dp[i] == optimum[i]) invalid[i] = invalid[i] || bad[i];
            }
        }
        std::size_t extended_checks = 0;
        for (std::size_t i = 0; i < rows; ++i) {
            if (optimum[i] == unreachable || invalid[i]) continue;
            if (result.entries[i].profit != optimum[i]) throw std::logic_error("graph conditional optimality failure");
            if (result.entries[i].head != no_node) ++extended_checks;
        }
        if (extended_checks == 0) throw std::logic_error("graph oracle checked only trivial states");
    }
    const auto logarithm = std::log2(static_cast<double>(rows));
    std::cout << rows << ',' << pairs.size() << ',' << count << ',' << stats.colorings << ','
              << stats.singleton_calls << ',' << stats.matrix_queries << ',' << std::fixed << std::setprecision(4)
              << static_cast<double>(stats.matrix_queries) / (static_cast<double>(rows) * logarithm) << ','
              << static_cast<double>(stats.matrix_queries) / (static_cast<double>(rows) * logarithm * logarithm) << '\n';
}

int main(int argc, char** argv) {
    try {
        std::uint64_t maximum_power = 8;
        if (argc == 2 && std::string_view(argv[1]) == "--help") {
            std::cout << "Usage: knapsack_extension_probe [--graph] [maximum root power, 2..8]\n"
                         "Counts matrix queries for two-color or complete-graph b=2 instances.\n";
            return 0;
        }
        const bool graph = argc >= 2 && std::string_view(argv[1]) == "--graph";
        const int position = graph ? 2 : 1;
        if (argc > position) maximum_power = knapsack::app::number(argv[position]);
        if (argc > position + 1 || maximum_power < 2 || maximum_power > 8)
            throw std::invalid_argument("expected maximum root power in 2..8");
        if (graph) std::cout << "rows,initial_sources,functions,colorings,singleton_calls,matrix_queries,queries_over_n_log_n,queries_over_n_log_squared_n\n";
        else std::cout << "rows,initial_sources,live_sources,first_queries,second_queries,queries_per_row,queries_over_claimed_scale\n";
        for (std::uint64_t power = 2; power <= maximum_power; ++power) {
            if (graph) graph_probe(std::size_t{1} << power);
            else probe(std::size_t{1} << power);
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }
}
