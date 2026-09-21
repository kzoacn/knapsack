#include "instance.hpp"

#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--help") {
            std::cout << "Usage: knapsack_generate [--n N] [--wmax W] [--seed S] [--capacity-percent P]\n"
                         "                         [--duplicate-percent P]\n"
                         "                         [--family uniform|correlated|subset|duplicates|ties]\n";
            return 0;
        }
        const auto config = knapsack::bench::arguments(argc, argv);
        const auto [items, capacity] = knapsack::bench::generate(config);
        std::cout << items.size() << ' ' << capacity << '\n';
        for (const auto& item : items) std::cout << item.weight << ' ' << item.profit << '\n';
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }
}
