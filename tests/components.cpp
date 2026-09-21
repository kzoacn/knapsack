#include "coloring.hpp"
#include "hinted.hpp"
#include "monge.hpp"

#include <algorithm>
#include <iostream>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>

void require(bool, const char*);

using namespace knapsack::detail;

void test_components() {
    std::mt19937_64 rng(404093);
    {
        const std::size_t rows = std::size_t{1} << 41;
        const std::size_t gaps = std::size_t{1} << 40;
        const EvenRowSampler sample(rows, gaps + 1);
        require(sample(0) == 0, "large sample first row");
        require(sample(gaps / 2) == (rows - 1) / 2, "large sample midpoint");
        require(sample(gaps) == rows - 1, "large sample last row without multiplication overflow");
    }
    for (std::size_t trial = 0; trial < 500; ++trial) {
        const auto nr = 1 + rng() % 100, nc = 1 + rng() % 40;
        std::vector<Score> a(nc), q(nr + nc + 1, 0);
        Score slope = 15;
        for (std::size_t i = 1; i < q.size(); ++i) { slope -= rng() % 4; q[i] = q[i - 1] + slope; }
        for (auto& value : a) value = static_cast<Score>(rng() % 101) - 50;
        auto matrix = [&](std::size_t i, std::size_t j) -> Score {
            return j > i ? unreachable : a[j] + q[i - j];
        };
        auto dense = smawk(nr, nc, matrix);
        auto compact = monge_envelope(nr, nc, matrix);
        auto tall = tall_smawk(nr, nc, matrix);
        std::vector<std::size_t> expanded(nr);
        std::vector<std::size_t> tall_expanded(nr);
        for (const auto& interval : compact)
            for (auto i = interval.begin; i < interval.end; ++i) expanded[i] = interval.column;
        for (const auto& interval : tall)
            for (auto i = interval.begin; i < interval.end; ++i) tall_expanded[i] = interval.column;
        for (std::size_t i = 0; i < nr; ++i) {
            std::size_t best = 0;
            for (std::size_t j = 1; j < nc; ++j) if (matrix(i, j) > matrix(i, best)) best = j;
            require(dense[i] == best, "SMAWK vs exhaustive matrix");
            require(expanded[i] == best, "compact Monge vs exhaustive matrix");
            require(tall_expanded[i] == best, "tall SMAWK vs exhaustive matrix");
        }
    }
    std::size_t queries = 0;
    const auto compact = tall_smawk(1'000'000'000, 4, [&](std::size_t row, std::size_t col) {
        ++queries;
        return Score{col} * static_cast<Score>(row) - Score{col * col} * 100'000'000;
    });
    require(compact.size() <= 4 && queries < 500, "tall matrix must remain compressed");

    for (std::size_t trial = 0; trial < 200; ++trial) {
        const auto universe = 1 + rng() % 100;
        HintSets sets(1 + rng() % 100);
        for (auto& s : sets) {
            std::set<std::size_t> distinct;
            const auto count = rng() % 9;
            for (std::size_t i = 0; i < count; ++i) distinct.insert(rng() % universe);
            s.assign(distinct.begin(), distinct.end());
        }
        const auto family = isolating_family(sets, universe);
        std::vector<bool> covered(sets.size());
        for (const auto& coloring : family) {
            for (auto i : coloring.isolated) {
                require(!covered[i], "isolation assigned twice");
                covered[i] = true;
                std::set<std::size_t> colors;
                for (auto x : sets[i]) colors.insert(coloring.colors[x]);
                require(colors.size() == sets[i].size(), "isolation certificate");
            }
        }
        require(std::all_of(covered.begin(), covered.end(), [](bool x) { return x; }), "isolation coverage");
        const auto balanced = balanced_coloring(sets, universe, 8);
        for (auto c : balanced) require(c < 8, "balanced color range");
    }
    {
        HintSets sets(20);
        for (auto& s : sets) {
            for (std::size_t x = 0; x < 5000; ++x) if (rng() % 2) s.push_back(x);
        }
        const auto colors = balanced_coloring(sets, 5000, 8);
        for (const auto& s : sets) {
            std::vector<std::size_t> counts(8);
            for (auto x : s) ++counts[colors[x]];
            require(*std::max_element(counts.begin(), counts.end()) < 1000, "nontrivial set balancing");
        }
    }

    // Exhaustively enumerate the UNRESTRICTED optimization problem, including
    // all tied maximizers. Only states satisfying Problem 1's universal
    // support-containment premise are required to equal that optimum.
    for (std::size_t trial = 0; trial < 700; ++trial) {
        const std::size_t n = 2 + rng() % 10, k = 1 + rng() % 3;
        std::vector<ConcaveFunction> functions(k);
        for (std::size_t f = 0; f < k; ++f) {
            auto& q = functions[f];
            q.weight = static_cast<std::uint32_t>(f + 1);
            Score slope = 1 + rng() % 8;
            for (std::size_t i = 1; i <= n; ++i) {
                slope -= rng() % 3;
                q.prefix.push_back(q.prefix.back() + slope);
            }
            q.tail_slope = slope;
            if (trial % 2 == 0) {
                q.max_count = rng() % (n + 1);
                q.prefix.resize(q.max_count + 1);
                q.tail_slope = -1'000'000;
            }
        }
        std::vector<ExtensionEntry> input(n);
        std::vector<std::size_t> hints(n);
        HintSets general_hints(n);
        std::vector<Score> initial(n);
        for (std::size_t i = 0; i < n; ++i) {
            input[i] = {rng() % 4 == 0 ? unreachable : static_cast<Score>(rng() % 13) - 6, i, no_node};
            hints[i] = rng() % (k + 1);
            if (hints[i] == k) hints[i] = no_node;
            initial[i] = input[i].profit;
            for (std::size_t f = 0; f < k; ++f) if (rng() % 2) general_hints[i].push_back(f);
        }
        knapsack::Statistics stats;
        std::vector<ExtensionNode> nodes;
        const auto result = extend_singletons(input, hints, functions, nodes, {}, stats);
        const auto general = extend_hinted(initial, general_hints, functions, {}, stats);
        for (std::size_t i = 0; i < n; ++i) {
            Score best = unreachable;
            bool all_valid = true;
            bool all_general_valid = true;
            for (std::size_t source = 0; source <= i; ++source) {
                if (input[source].profit == unreachable) continue;
                auto visit = [&](auto&& self, std::size_t f, std::size_t weight, Score profit, bool valid, bool general_valid) -> void {
                    if (f == k) {
                        if (weight != i) return;
                        if (profit > best) { best = profit; all_valid = valid; all_general_valid = general_valid; }
                        else if (profit == best) {
                            all_valid = all_valid && valid;
                            all_general_valid = all_general_valid && general_valid;
                        }
                        return;
                    }
                    for (std::size_t count = 0; count <= functions[f].max_count &&
                         weight + count * functions[f].weight <= i; ++count)
                        self(self, f + 1, weight + count * functions[f].weight,
                             profit + functions[f](count), valid && (count == 0 || hints[source] == f),
                             general_valid && (count == 0 || std::find(general_hints[source].begin(),
                                  general_hints[source].end(), f) != general_hints[source].end()));
                };
                visit(visit, 0, source, input[source].profit, true, true);
            }
            if (all_valid && best != unreachable)
                require(result[i].profit == best, "singleton conditional optimality");
            if (all_general_valid && best != unreachable)
                require(general.entries[i].profit == best, "general extension conditional optimality");
            if (general.entries[i].profit != unreachable) {
                const auto& entry = general.entries[i];
                auto value = initial[entry.origin];
                auto weight = entry.origin;
                std::vector<bool> used(k);
                for (auto node = entry.head; node != no_node; node = general.nodes[node].parent) {
                    const auto& step = general.nodes[node];
                    require(step.count <= functions[step.function].max_count, "physical multiplicity in extension");
                    require(!used[step.function], "extension used a function twice");
                    used[step.function] = true;
                    require(std::find(general_hints[entry.origin].begin(), general_hints[entry.origin].end(),
                                      step.function) != general_hints[entry.origin].end(), "general extension support");
                    value += functions[step.function](step.count);
                    weight += functions[step.function].weight * step.count;
                }
                require(value == entry.profit && weight == i, "general extension witness");
            }
            if (result[i].profit == unreachable) continue;
            auto value = input[result[i].origin].profit;
            auto weight = result[i].origin;
            for (auto node = result[i].head; node != no_node; node = nodes[node].parent) {
                const auto& step = nodes[node];
                require(step.count <= functions[step.function].max_count, "physical multiplicity in singleton");
                require(step.function == hints[result[i].origin], "singleton support");
                value += functions[step.function](step.count);
                weight += functions[step.function].weight * step.count;
            }
            require(weight == i && value == result[i].profit, "singleton witness");
        }
    }
    {
        std::vector<Score> input(21, unreachable);
        input[0] = 0;
        HintSets hints(input.size());
        std::vector<ConcaveFunction> functions(40);
        for (std::size_t f = 0; f < functions.size(); ++f) {
            hints[0].push_back(f);
            functions[f] = {static_cast<std::uint32_t>(f + 1), {0}, static_cast<Score>((f + 1) * 2)};
        }
        knapsack::Statistics stats;
        const auto result = extend_hinted(input, hints, functions, {}, stats);
        for (std::size_t i = 0; i < input.size(); ++i)
            require(result.entries[i].profit == static_cast<Score>(2 * i), "two-level extension branch");
        require(stats.colorings > 1, "two-level coloring exercised");
    }
    {
        // Widely separated source intervals must be searched independently.
        constexpr std::size_t length = 100'000, spacing = 100;
        std::vector<ExtensionEntry> input(length);
        std::vector<std::size_t> hints(length, no_node);
        for (std::size_t i = 0; i < length; i += spacing) {
            input[i] = {0, i, no_node};
            hints[i] = 0;
        }
        const std::vector<ConcaveFunction> functions{{1, {0, 1}, -1'000'000, 1}};
        knapsack::Statistics stats;
        std::vector<ExtensionNode> nodes;
        const auto result = extend_singletons(input, hints, functions, nodes, {}, stats);
        for (std::size_t i = 0; i < length; ++i) {
            const auto expected = i % spacing == 0 ? Score{0} : i % spacing == 1 ? Score{1} : unreachable;
            require(result[i].profit == expected, "disconnected multiplicity intervals");
        }
        require(stats.matrix_queries <= 4 * (length / spacing), "bounded interval search complexity");
    }
    {
        // Regression for the complexity audit: composition propagates hint
        // handles to new indices. The number of live sources can grow.
        std::vector<Score> input(100, unreachable);
        input[0] = 0;
        HintSets hints(input.size());
        hints[0] = {0, 1};
        const std::vector<ConcaveFunction> functions{{1, {0}, 10}, {2, {0}, 11}};
        knapsack::Statistics stats;
        const auto result = extend_hinted(input, hints, functions, {}, stats);
        require(result.entries.back().profit == 990, "composed singleton value");
        require(stats.peak_singleton_sources == 100, "live hint source growth");
    }
    std::cout << "Matrix, coloring, and extension contracts passed\n";
}
