#include "coloring.hpp"

#include <algorithm>
#include <bit>
#include <numeric>
#include <stdexcept>

namespace knapsack::detail {

std::vector<IsolatingColoring> isolating_family(const HintSets& sets,
                                               std::size_t universe_size) {
    std::size_t bound = 1;
    for (const auto& s : sets) bound = std::max(bound, s.size());
    if (bound > std::numeric_limits<std::size_t>::max() / bound)
        throw std::length_error("color count overflow");
    const auto colors = bound * bound;
    std::vector<std::size_t> pending(sets.size());
    std::iota(pending.begin(), pending.end(), 0);
    std::vector<IsolatingColoring> result;
    while (!pending.empty()) {
        std::vector<std::vector<std::size_t>> incidence(universe_size), used(pending.size());
        for (std::size_t i = 0; i < pending.size(); ++i)
            for (auto x : sets[pending[i]]) {
                if (x >= universe_size) throw std::invalid_argument("hint outside universe");
                incidence[x].push_back(i);
            }
        std::vector<bool> collided(pending.size(), false);
        IsolatingColoring coloring{std::vector<std::size_t>(universe_size, 0), {}, colors};
        std::vector<Score> penalties(colors);
        for (std::size_t x = 0; x < universe_size; ++x) {
            if (incidence[x].empty()) continue;
            std::fill(penalties.begin(), penalties.end(), 0);
            for (auto i : incidence[x]) {
                if (collided[i]) continue;
                const Score remaining = static_cast<Score>(sets[pending[i]].size() - used[i].size() - 1);
                const Score assigned = static_cast<Score>(used[i].size() + 1);
                const Score good_estimate = assigned * remaining + remaining * (remaining - 1) / 2;
                const Score surcharge = static_cast<Score>(colors) - good_estimate;
                for (auto c : used[i]) penalties[c] += surcharge;
            }
            const auto color = static_cast<std::size_t>(
                std::min_element(penalties.begin(), penalties.end()) - penalties.begin());
            coloring.colors[x] = color;
            for (auto i : incidence[x]) {
                if (collided[i]) continue;
                if (std::find(used[i].begin(), used[i].end(), color) != used[i].end()) collided[i] = true;
                else used[i].push_back(color);
            }
        }
        std::vector<std::size_t> next;
        for (std::size_t i = 0; i < pending.size(); ++i) {
            if (collided[i]) next.push_back(pending[i]);
            else coloring.isolated.push_back(pending[i]);
        }
        if (next.size() * 2 >= pending.size())
            throw std::logic_error("integer isolation estimator failed its half-cover certificate");
        result.push_back(std::move(coloring));
        pending = std::move(next);
    }
    return result;
}

std::vector<std::size_t> balanced_coloring(const HintSets& sets,
                                          std::size_t universe_size, std::size_t r) {
    if (!std::has_single_bit(r)) throw std::invalid_argument("color count must be a power of two");
    std::vector<std::size_t> answer(universe_size, 0), universe(universe_size);
    std::vector<int> sign(universe_size, 0);
    std::vector<std::vector<std::size_t>> incidence(universe_size);
    std::iota(universe.begin(), universe.end(), 0);
    const auto h = std::max<std::size_t>(1, std::bit_width(sets.size() * 2));
    auto split = [&](auto&& self, const HintSets& local, const std::vector<std::size_t>& elements,
                     std::size_t first_color, std::size_t count) -> void {
        if (count == 1 || elements.empty()) {
            for (auto x : elements) answer[x] = first_color;
            return;
        }
        std::size_t b = 0;
        for (const auto& s : local) b = std::max(b, s.size());
        const auto discrepancy = 8 * ceil_sqrt(UnsignedScore{b} * h);
        if (discrepancy >= b) {
            for (std::size_t i = 0; i < elements.size(); ++i) sign[elements[i]] = (i % 2 ? -1 : 1);
        } else {
            // Integer upper approximations to the exponential estimator.
            // t=(k+1)/k; multiplying by t/cosh(log t) or its reciprocal
            // uses the common integer denominator d. Their mean is one.
            // Round upward at each update. With scale >= b, total rounding
            // increases the potential by at most its initial value.
            const Score k = static_cast<Score>(ceil_sqrt((UnsignedScore{b} + h - 1) / h));
            const Score denominator = k * k + (k + 1) * (k + 1);
            const Score up = 2 * (k + 1) * (k + 1), down = 2 * k * k;
            const Score scale = static_cast<Score>(b) + 1;
            const Score budget = checked_positive_product(4 * static_cast<Score>(local.size()), scale);
            checked_positive_product(budget + 1, up + 1);
            std::vector<Score> plus(local.size(), scale), minus(local.size(), scale);
            for (auto x : elements) incidence[x].clear();
            for (std::size_t i = 0; i < local.size(); ++i) {
                for (auto x : local[i]) incidence[x].push_back(i);
            }
            for (auto x : elements) {
                Score delta = 0;
                for (auto i : incidence[x]) delta += plus[i] - minus[i];
                sign[x] = delta <= 0 ? 1 : -1;
                for (auto i : incidence[x]) {
                    plus[i] = (plus[i] * (sign[x] > 0 ? up : down) + denominator - 1) / denominator;
                    minus[i] = (minus[i] * (sign[x] > 0 ? down : up) + denominator - 1) / denominator;
                }
            }
        }
        HintSets left(local.size()), right(local.size());
        for (std::size_t i = 0; i < local.size(); ++i) {
            for (auto x : local[i]) (sign[x] > 0 ? left[i] : right[i]).push_back(x);
            const auto a = left[i].size(), b_size = right[i].size();
            const auto difference = a >= b_size ? a - b_size : b_size - a;
            if (difference > discrepancy)
                throw std::runtime_error("set balancing failed its exact discrepancy certificate");
        }
        std::vector<std::size_t> left_elements, right_elements;
        for (auto x : elements) (sign[x] > 0 ? left_elements : right_elements).push_back(x);
        self(self, left, left_elements, first_color, count / 2);
        self(self, right, right_elements, first_color + count / 2, count / 2);
    };
    split(split, sets, universe, 0, r);
    return answer;
}

} // namespace knapsack::detail
