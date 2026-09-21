#include "knapsack/knapsack.hpp"
#include "numeric.hpp"
#include "selection.hpp"
#include "solver_testing.hpp"

#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_components();

template<class F>
void must_throw(F&& action, const char* description) {
    bool threw = false;
    try { action(); } catch (const std::exception&) { threw = true; }
    require(threw, description);
}

void check_instance(const std::vector<knapsack::Item>& items, std::uint64_t capacity, bool brute) {
    try {
        const auto expected = knapsack::solve_dp(items, capacity);
        auto check = [&](const knapsack::Solution& actual, const char* label) {
            if (actual.profit != expected.profit)
                throw std::runtime_error(std::string(label) + " profit=" + std::to_string(actual.profit) +
                                         "; expected=" + std::to_string(expected.profit));
            if (!knapsack::validate_solution(items, capacity, actual))
                throw std::runtime_error(std::string(label) + " invalid witness");
        };
        check(expected, "capacity DP");
        if (brute) check(knapsack::solve_brute_force(items, capacity), "exhaustive oracle");
        const auto exchange = knapsack::solve_exchange(items, capacity);
        const auto paper = knapsack::solve_paper(items, capacity);
        check(exchange, "exchange");
        check(paper, "paper");
    } catch (...) {
        std::cerr << "Reproduction input:\n" << items.size() << ' ' << capacity << '\n';
        for (const auto& item : items) std::cerr << item.weight << ' ' << item.profit << '\n';
        throw;
    }
}

int main() {
    try {
        test_components();
        std::mt19937_64 random(230804093);
        for (std::size_t n = 1; n < 400; ++n) {
            std::vector<int> values(n);
            for (auto& v : values) v = static_cast<int>(random() % 29);
            auto sorted = values;
            std::sort(sorted.begin(), sorted.end());
            const auto k = static_cast<std::size_t>(random() % n);
            knapsack::detail::select_nth<int>(values, k, std::less<int>{});
            require(values[k] == sorted[k], "deterministic selection");
            for (std::size_t i = 0; i < k; ++i) require(values[i] <= values[k], "selection left partition");
            for (std::size_t i = k + 1; i < n; ++i) require(values[i] >= values[k], "selection right partition");
        }
        for (std::size_t trial = 0; trial < 1000; ++trial) {
            std::vector<knapsack::Item> items(random() % 17);
            std::uint64_t total = 0;
            for (auto& item : items) {
                item = {static_cast<std::uint32_t>(1 + random() % 30), 1 + random() % 80};
                total += item.weight;
            }
            const auto capacity = random() % (total + 2);
            try { check_instance(items, capacity, true); }
            catch (...) {
                std::cerr << "random seed=230804093, trial=" << trial << '\n';
                throw;
            }
        }
        require(knapsack::detail::ceil_sqrt(0) == 0, "sqrt zero");
        require(knapsack::detail::ceil_sqrt(17) == 5, "sqrt ceiling");
        check_instance({}, 0, true);
        check_instance({{1, 1}}, 0, true);
        check_instance({{2, 3}, {3, 4}, {5, 8}}, 5, true);
        check_instance({{3, 4}, {4, 5}, {2, 3}, {5, 8}}, 7, true);
        check_instance({{5, 10}, {4, 8}, {3, 6}, {2, 4}, {1, 2}}, 8, true);
        check_instance({{7, 10}, {7, 10}, {7, 10}, {7, 10}}, 20, true);
        check_instance({{1, std::uint64_t{1} << 60}, {2, (std::uint64_t{1} << 60) + 1}, {3, 1}}, 3, true);
        for (std::size_t trial = 0; trial < 40; ++trial) {
            std::vector<knapsack::Item> items(80 + random() % 170);
            std::uint64_t total = 0;
            for (auto& item : items) {
                item.weight = static_cast<std::uint32_t>(1 + random() % 35);
                item.profit = trial % 3 == 0 ? item.weight : 1 + random() % 100;
                total += item.weight;
            }
            check_instance(items, total * (10 + random() % 81) / 100, false);
            if (trial < 10) {
                knapsack::Statistics stats;
                const auto layered = knapsack::detail::solve_partitioned_reference(items, total / 2, 4, stats);
                require(layered.profit == knapsack::solve_dp(items, total / 2).profit, "multiple weight layers");
                require(knapsack::validate_solution(items, total / 2, layered), "multiple layers witness");
                require(stats.weight_layers == 4, "multiple layers exercised");
            }
        }
        std::vector<knapsack::Item> repeated(400, {3, 1});
        for (std::size_t i = 0; i < repeated.size(); ++i) repeated[i].profit = 1 + i % 23;
        check_instance(repeated, 607, false);
        {
            const std::vector<knapsack::Item> items{{100, 3}, {101, 4}};
            knapsack::Statistics stats;
            require(knapsack::solve_paper(items, 101, {}, &stats).profit == 4 && stats.capacity_shortcut,
                    "large-weight capacity shortcut");
        }
        must_throw([] { knapsack::solve_paper(std::vector<knapsack::Item>{{0, 1}}, 2); }, "zero weight rejected");
        must_throw([] { knapsack::solve_dp(std::vector<knapsack::Item>{{1, 0}}, 2); }, "zero profit rejected");
        must_throw([] { knapsack::solve_paper(std::vector<knapsack::Item>{{1, UINT64_MAX}, {1, 1}}, 2); }, "aggregate overflow rejected");
        must_throw([] { knapsack::solve_paper(std::vector<knapsack::Item>{{2, 1}, {2, 2}}, 3, {1, 100}); }, "state limit enforced");
        must_throw([] { knapsack::solve_dp(std::vector<knapsack::Item>{{1, 1}, {1, 2}}, 1, {10, 0}); }, "trace limit enforced");
        std::cout << "All correctness tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
