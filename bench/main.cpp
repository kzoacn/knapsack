#include "instance.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>

#if defined(__linux__)
#include <sys/resource.h>
#endif

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--help") {
            std::cout << "Usage: knapsack_bench [generator options] [--algorithm paper|exchange|dp|brute]\n"
                         "                      [--repeats N] [--verify]\n"
                         "CSV RSS is the process high-water mark in KiB on Linux; zero on other platforms.\n";
            return 0;
        }
        const auto config = knapsack::bench::arguments(argc, argv);
        const auto [items, capacity] = knapsack::bench::generate(config);
        const auto maximum_weight_items = std::count_if(items.begin(), items.end(),
            [&](const auto& item) { return item.weight == config.max_weight; });
        std::uint64_t expected = 0;
        if (config.verify) expected = knapsack::solve_dp(items, capacity).profit;
        std::cout << "algorithm,n,wmax,capacity,seed,family,repetition,profit,elapsed_ms,peak_states,trace_nodes,"
                     "matrix_queries,singleton_calls,colorings,process_peak_rss_kib,capacity_percent,forced_duplicate_percent,max_weight_items\n";
        for (std::size_t repetition = 0; repetition < config.repeats; ++repetition) {
            knapsack::Statistics stats;
            const auto start = std::chrono::steady_clock::now();
            const auto solution = knapsack::app::dispatch(config.algorithm, items, capacity, {}, stats);
            const auto end = std::chrono::steady_clock::now();
            if (!knapsack::validate_solution(items, capacity, solution) ||
                (config.verify && solution.profit != expected)) throw std::logic_error("benchmark correctness failure");
            std::uint64_t rss = 0;
#if defined(__linux__)
            rusage usage{};
            if (getrusage(RUSAGE_SELF, &usage) == 0) rss = static_cast<std::uint64_t>(usage.ru_maxrss);
#endif
            const std::chrono::duration<double, std::milli> elapsed = end - start;
            std::cout << config.algorithm << ',' << items.size() << ',' << config.max_weight << ',' << capacity
                      << ',' << config.seed << ',' << config.family << ',' << repetition << ',' << solution.profit
                      << ',' << std::fixed << std::setprecision(3) << elapsed.count() << ',' << stats.peak_states
                      << ',' << stats.trace_nodes << ',' << stats.matrix_queries << ',' << stats.singleton_calls
                      << ',' << stats.colorings << ',' << rss << ',' << config.capacity_percent
                      << ',' << config.duplicate_percent << ',' << maximum_weight_items << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }
}
