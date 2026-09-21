#pragma once

#include "common.hpp"

#include <random>

namespace knapsack::bench {

struct Config {
    std::size_t n = 100;
    std::uint32_t max_weight = 20;
    std::uint64_t seed = 230804093;
    std::uint64_t capacity_percent = 50;
    std::uint64_t duplicate_percent = 0;
    std::string family = "uniform";
    std::string algorithm = "paper";
    std::size_t repeats = 1;
    bool verify = false;
};

inline Config arguments(int argc, char** argv) {
    Config config;
    bool duplicate_explicit = false;
    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];
        if (option == "--verify") { config.verify = true; continue; }
        if (++i == argc) throw std::invalid_argument("missing value for " + option);
        if (option == "--family") config.family = argv[i];
        else if (option == "--algorithm") config.algorithm = argv[i];
        else if (option == "--n") config.n = app::number(argv[i]);
        else if (option == "--seed") config.seed = app::number(argv[i]);
        else if (option == "--capacity-percent") config.capacity_percent = app::number(argv[i]);
        else if (option == "--duplicate-percent") {
            config.duplicate_percent = app::number(argv[i]);
            duplicate_explicit = true;
        }
        else if (option == "--repeats") config.repeats = app::number(argv[i]);
        else if (option == "--wmax") {
            const auto value = app::number(argv[i]);
            if (value == 0 || value > std::numeric_limits<std::uint32_t>::max())
                throw std::invalid_argument("wmax must fit positive uint32_t");
            config.max_weight = static_cast<std::uint32_t>(value);
        } else throw std::invalid_argument("unknown option: " + option);
    }
    if (config.n > 10'000'000 || config.capacity_percent > 100 || config.duplicate_percent > 100 || config.repeats == 0)
        throw std::invalid_argument("require n<=10000000, percentages<=100 and repeats>0");
    if (config.family == "duplicates") {
        if (duplicate_explicit && config.duplicate_percent != 100)
            throw std::invalid_argument("duplicates family requires duplicate-percent 100");
        config.duplicate_percent = 100;
    }
    return config;
}

inline std::pair<std::vector<Item>, std::uint64_t> generate(const Config& config) {
    if (config.family != "uniform" && config.family != "correlated" &&
        config.family != "subset" && config.family != "duplicates" && config.family != "ties")
        throw std::invalid_argument("family must be uniform, correlated, subset, duplicates, or ties");
    std::mt19937_64 random(config.seed);
    std::vector<Item> items(config.n);
    std::uint64_t total_weight = 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
        auto& item = items[i];
        item.weight = static_cast<std::uint32_t>(1 + random() % config.max_weight);
        item.profit = 1 + random() % (std::uint64_t{4} * config.max_weight);
        // Spread exactly floor(n*p/100) forced copies through the instance.
        // Remaining random items can naturally have the same weight too.
        if ((i + 1) * config.duplicate_percent / 100 > i * config.duplicate_percent / 100 ||
            config.family == "duplicates") item.weight = config.max_weight;
        if (config.family == "correlated") item.profit = item.weight + config.max_weight;
        if (config.family == "subset") item.profit = item.weight;
        if (config.family == "ties") item.profit = std::uint64_t{7} * item.weight;
        total_weight += item.weight;
    }
    // Quotient/remainder form avoids multiplying a potentially large sum by 100.
    const auto capacity = total_weight / 100 * config.capacity_percent +
                          total_weight % 100 * config.capacity_percent / 100;
    return {std::move(items), capacity};
}

} // namespace knapsack::bench
