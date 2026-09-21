#include "common.hpp"

#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    try {
        std::string algorithm = "paper", path;
        knapsack::Limits limits;
        bool show_statistics = false;
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--help") {
                std::cout << "Usage: knapsack_cli [--algorithm paper|exchange|dp|brute] [--input FILE]\n"
                             "                    [--max-states N] [--max-traces N] [--stats]\n"
                             "Input: n capacity, followed by n lines of weight profit. Indices are zero-based.\n";
                return 0;
            }
            if (option == "--stats") { show_statistics = true; continue; }
            if (++i == argc) throw std::invalid_argument("missing value for " + option);
            if (option == "--algorithm") algorithm = argv[i];
            else if (option == "--input") path = argv[i];
            else if (option == "--max-states") limits.max_states = knapsack::app::number(argv[i]);
            else if (option == "--max-traces") limits.max_trace_nodes = knapsack::app::number(argv[i]);
            else throw std::invalid_argument("unknown option: " + option);
        }
        std::ifstream file;
        if (!path.empty()) { file.open(path); if (!file) throw std::runtime_error("cannot open input file: " + path); }
        const auto [items, capacity] = knapsack::app::read_instance(path.empty() ? std::cin : file);
        knapsack::Statistics statistics;
        const auto solution = knapsack::app::dispatch(algorithm, items, capacity, limits, statistics);
        knapsack::app::write_solution(std::cout, solution);
        if (show_statistics) {
            std::cerr << "{\"algorithm\":\"" << algorithm << "\",\"matrix_queries\":" << statistics.matrix_queries
                      << ",\"candidate_intervals\":" << statistics.candidate_intervals
                      << ",\"bucket_visits\":" << statistics.bucket_visits
                      << ",\"singleton_calls\":" << statistics.singleton_calls
                      << ",\"peak_singleton_sources\":" << statistics.peak_singleton_sources
                      << ",\"colorings\":" << statistics.colorings
                      << ",\"rank_phases\":" << statistics.rank_phases
                      << ",\"weight_layers\":" << statistics.weight_layers
                      << ",\"peak_states\":" << statistics.peak_states
                      << ",\"trace_nodes\":" << statistics.trace_nodes
                      << ",\"capacity_shortcut\":" << (statistics.capacity_shortcut ? "true" : "false") << "}\n";
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }
}
