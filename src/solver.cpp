#include "knapsack/knapsack.hpp"
#include "hinted.hpp"
#include "monge.hpp"
#include "numeric.hpp"
#include "selection.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace knapsack::detail {
namespace {

// Deliberately conservative constants; derivation is recorded in docs/numerics.md.
constexpr std::uint64_t proximity_constant = std::uint64_t{1} << 25;

struct PreparedItem {
    std::size_t original;
    std::uint32_t weight;
    Score profit;
    bool greedy = false;
};
struct Group {
    std::uint32_t weight;
    std::vector<std::size_t> add, remove;
    std::size_t layer = 0;
};
struct Prepared {
    std::vector<PreparedItem> items;
    std::vector<Group> groups;
    std::uint32_t max_weight = 0;
    std::uint64_t greedy_weight = 0, total_weight = 0, radius_cap = 0;
    Score total_profit = 0;
    std::size_t layer_count = 1;
};
struct Trace {
    std::size_t parent, group, offset, count;
    bool remove;
};
struct State {
    Score profit = unreachable;
    std::size_t trace = no_node;
    std::vector<std::size_t> positive, negative;
};
struct Table {
    std::size_t radius = 0;
    std::vector<State> entries{1};
};

Prepared prepare(std::span<const Item> input, std::uint64_t capacity, bool layers) {
    Prepared p;
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i].weight > capacity) continue;
        p.items.push_back({i, input[i].weight, static_cast<Score>(input[i].profit), false});
        p.max_weight = std::max(p.max_weight, input[i].weight);
        p.total_weight += input[i].weight;
    }
    if (p.items.empty()) return p;
    const Score n = static_cast<Score>(p.items.size());
    const Score multiplier = 1 + n + checked_positive_product(n, n + 1) / 2;
    for (std::size_t i = 0; i < p.items.size(); ++i) {
        auto& item = p.items[i];
        const auto base = checked_positive_product(item.profit, multiplier);
        if (base > score_limit - static_cast<Score>(i) - 1) throw std::overflow_error("profit perturbation");
        item.profit = checked_positive_product(base + static_cast<Score>(i) + 1, p.max_weight) + 1;
        checked_positive_product(item.profit, p.max_weight); // Efficiency cross-products.
        if (p.total_profit > score_limit - item.profit) throw std::overflow_error("total perturbed profit");
        p.total_profit += item.profit;
    }
    p.radius_cap = static_cast<std::uint64_t>(std::min(
        UnsignedScore{2} * p.max_weight * p.max_weight, UnsignedScore{p.total_weight}));
    auto efficient = [&](std::size_t a, std::size_t b) {
        return p.items[a].profit * p.items[b].weight > p.items[b].profit * p.items[a].weight;
    };
    std::vector<std::size_t> order(p.items.size());
    std::iota(order.begin(), order.end(), 0);
    auto undecided = std::span<std::size_t>(order);
    auto remaining = capacity;
    while (!undecided.empty()) {
        const auto middle = undecided.size() / 2;
        select_nth(undecided, middle, efficient);
        std::uint64_t weight = 0;
        for (std::size_t i = 0; i < middle; ++i) weight += p.items[undecided[i]].weight;
        if (weight > remaining) { undecided = undecided.first(middle); continue; }
        for (std::size_t i = 0; i < middle; ++i) p.items[undecided[i]].greedy = true;
        p.greedy_weight += weight;
        remaining -= weight;
        const auto pivot = undecided[middle];
        if (p.items[pivot].weight > remaining) break;
        p.items[pivot].greedy = true;
        remaining -= p.items[pivot].weight;
        p.greedy_weight += p.items[pivot].weight;
        undecided = undecided.subspan(middle + 1);
    }

    // Bucketing is linear in n + w_max. Only O(w_max) boundary representatives
    // and O(w_max) candidates PER weight class are sorted, never all n items.
    std::vector<std::vector<std::size_t>> buckets(static_cast<std::size_t>(p.max_weight) + 1);
    for (std::size_t i = 0; i < p.items.size(); ++i) buckets[p.items[i].weight].push_back(i);
    std::vector<std::size_t> inside, outside;
    for (std::size_t w = 1; w < buckets.size(); ++w) {
        if (buckets[w].empty()) continue;
        Group group{static_cast<std::uint32_t>(w), {}, {}, 0};
        for (auto i : buckets[w]) (p.items[i].greedy ? group.remove : group.add).push_back(i);
        auto rank = [&](std::vector<std::size_t>& ids, bool removal) {
            auto better = [&](std::size_t a, std::size_t b) {
                return removal ? p.items[a].profit < p.items[b].profit : p.items[a].profit > p.items[b].profit;
            };
            const auto keep = static_cast<std::size_t>(std::min<std::uint64_t>(ids.size(), 2ULL * p.max_weight));
            if (keep < ids.size()) { select_nth(std::span(ids), keep, better); ids.resize(keep); }
            std::sort(ids.begin(), ids.end(), better);
        };
        rank(group.add, false);
        rank(group.remove, true);
        if (!group.add.empty()) outside.push_back(p.groups.size());
        if (!group.remove.empty()) inside.push_back(p.groups.size());
        p.groups.push_back(std::move(group));
    }
    if (!layers) return p;
    std::sort(inside.begin(), inside.end(), [&](auto a, auto b) {
        return efficient(p.groups[b].remove.front(), p.groups[a].remove.front());
    });
    std::sort(outside.begin(), outside.end(), [&](auto a, auto b) {
        return efficient(p.groups[a].add.front(), p.groups[b].add.front());
    });
    const auto h = std::bit_width(static_cast<std::uint64_t>(p.max_weight) * 2);
    UnsignedScore quota = UnsignedScore{4} * proximity_constant * ceil_sqrt(UnsignedScore{p.max_weight} * h);
    std::size_t layer = 0, left = 0, right = 0;
    std::vector<bool> assigned(p.groups.size(), false);
    for (;;) {
        const auto count = static_cast<std::size_t>(std::min(quota, UnsignedScore{p.groups.size()}));
        while (left < std::min(count, inside.size())) {
            const auto g = inside[left++];
            if (!assigned[g]) { p.groups[g].layer = layer; assigned[g] = true; }
        }
        while (right < std::min(count, outside.size())) {
            const auto g = outside[right++];
            if (!assigned[g]) { p.groups[g].layer = layer; assigned[g] = true; }
        }
        if (left == inside.size() && right == outside.size()) break;
        quota *= 2;
        ++layer;
    }
    p.layer_count = layer + 1;
    return p;
}

std::size_t checked_radius(const Prepared& p, UnsignedScore bound, Limits limits) {
    bound = std::min(bound, UnsignedScore{p.radius_cap});
    if (limits.max_states == 0 || bound > (limits.max_states - 1) / 2)
        throw std::length_error("exchange table exceeds max_states; increase the limit or select capacity DP");
    const auto radius = static_cast<std::size_t>(bound);
    checked_positive_product(p.total_profit + 1, static_cast<Score>(radius) * 8 + 16);
    return radius;
}

void resize(Table& table, std::size_t radius, Statistics& stats) {
    if (radius == table.radius) return;
    std::vector<State> entries(radius * 2 + 1);
    const auto common = std::min(radius, table.radius);
    for (std::size_t i = 0; i <= 2 * common; ++i)
        entries[radius - common + i] = std::move(table.entries[table.radius - common + i]);
    table.entries = std::move(entries);
    table.radius = radius;
    stats.peak_states = std::max(stats.peak_states, table.entries.size());
}

std::size_t append(std::vector<Trace>& traces, Trace trace, Limits limits, Statistics& stats) {
    if (traces.size() >= limits.max_trace_nodes) throw std::length_error("solver exceeds max_trace_nodes");
    traces.push_back(trace);
    stats.trace_nodes = std::max(stats.trace_nodes, traces.size());
    return traces.size() - 1;
}

ConcaveFunction function(const Prepared& p, std::size_t group, bool removal,
                         std::size_t offset, std::size_t count) {
    const auto& g = p.groups[group];
    const auto& items = removal ? g.remove : g.add;
    ConcaveFunction q{g.weight, {0}, -(2 * p.total_profit + 1), 0};
    if (offset >= items.size()) return q;
    count = std::min(count, items.size() - offset);
    q.max_count = count;
    q.prefix.reserve(count + 1);
    for (std::size_t i = offset; i < offset + count; ++i)
        q.prefix.push_back(q.prefix.back() + (removal ? -p.items[items[i]].profit : p.items[items[i]].profit));
    return q;
}

void batch_update(Table& table, const Prepared& p, std::size_t group, bool removal,
                   std::vector<Trace>& traces, Limits limits, Statistics& stats) {
    const auto& ids = removal ? p.groups[group].remove : p.groups[group].add;
    if (ids.empty()) return;
    const auto q = function(p, group, removal, 0, ids.size());
    const auto size = table.entries.size();
    auto index = [&](std::size_t i) { return removal ? size - 1 - i : i; };
    std::vector<State> result(size);
    for (std::size_t r = 0; r < std::min<std::size_t>(q.weight, size); ++r) {
        std::vector<std::size_t> sources;
        for (std::size_t i = r; i < size; i += q.weight)
            if (table.entries[index(i)].profit != unreachable) sources.push_back(i);
        if (sources.empty()) continue;
        const auto rows = (size - 1 - r) / q.weight + 1;
        auto evaluate = [&](std::size_t row, std::size_t col) {
            ++stats.matrix_queries;
            const auto target = r + row * q.weight, source = sources[col];
            if (target < source) return unreachable;
            return table.entries[index(source)].profit + q((target - source) / q.weight);
        };
        const auto maxima = smawk(rows, sources.size(), evaluate);
        for (std::size_t row = 0; row < rows; ++row) {
            const auto target = r + row * q.weight, source = sources[maxima[row]];
            if (target < source) continue;
            const auto count = (target - source) / q.weight;
            if (count >= q.prefix.size()) continue;
            const auto& old = table.entries[index(source)];
            auto& state = result[index(target)];
            state.profit = old.profit + q(count);
            state.trace = count ? append(traces, {old.trace, group, 0, count, removal}, limits, stats) : old.trace;
        }
    }
    table.entries = std::move(result);
}

void rank_update(Table& table, const Prepared& p, std::span<const std::size_t> primary,
                 std::size_t offset, std::size_t band, std::size_t next_hint_bound, bool removal,
                 std::vector<Trace>& traces, Limits limits, Statistics& stats) {
    const auto size = table.entries.size();
    auto index = [&](std::size_t i) { return removal ? size - 1 - i : i; };
    std::vector<Score> profits(size);
    HintSets hints(size);
    std::vector<ConcaveFunction> functions;
    for (auto g : primary) functions.push_back(function(p, g, removal, offset, band));
    for (std::size_t i = 0; i < size; ++i) {
        const auto& old = table.entries[index(i)];
        profits[i] = old.profit;
        hints[i] = removal ? old.negative : old.positive;
    }
    auto extension = extend_hinted(profits, hints, functions, limits, stats);
    std::vector<State> result(size);
    for (std::size_t i = 0; i < size; ++i) {
        const auto& entry = extension.entries[i];
        if (entry.profit == unreachable) continue;
        bool valid = true;
        std::vector<std::size_t> next;
        for (auto node = entry.head; node != no_node; node = extension.nodes[node].parent) {
            const auto& step = extension.nodes[node];
            const auto actual = functions[step.function].prefix.size() - 1;
            if (step.count > actual) { valid = false; break; }
            const auto& group = p.groups[primary[step.function]];
            const auto available = removal ? group.remove.size() : group.add.size();
            if (step.count == actual && available > offset + band) next.push_back(step.function);
        }
        if (!valid || next.size() > next_hint_bound) continue;
        const auto& old = table.entries[index(entry.origin)];
        auto& state = result[index(i)];
        state.profit = entry.profit;
        state.trace = old.trace;
        for (auto node = entry.head; node != no_node; node = extension.nodes[node].parent) {
            const auto& step = extension.nodes[node];
            state.trace = append(traces, {state.trace, primary[step.function], offset, step.count, removal}, limits, stats);
        }
        if (removal) { state.positive = old.positive; state.negative = std::move(next); }
        else { state.negative = old.negative; state.positive = std::move(next); }
    }
    table.entries = std::move(result);
}

Solution finish(std::span<const Item> input, std::uint64_t capacity, const Prepared& p,
                 const Table& table, const std::vector<Trace>& traces) {
    const auto remaining = capacity - p.greedy_weight;
    const auto last = table.radius + std::min<std::uint64_t>(table.radius, remaining);
    auto best = no_node;
    for (std::size_t i = 0; i <= last; ++i)
        if (table.entries[i].profit != unreachable &&
            (best == no_node || table.entries[i].profit > table.entries[best].profit)) best = i;
    if (best == no_node) throw std::logic_error("optimal exchange was lost");
    std::vector<bool> selected(input.size(), false);
    for (const auto& item : p.items) if (item.greedy) selected[item.original] = true;
    for (auto node = table.entries[best].trace; node != no_node; node = traces[node].parent) {
        const auto& trace = traces[node];
        const auto& group = p.groups[trace.group];
        const auto& ids = trace.remove ? group.remove : group.add;
        if (trace.offset + trace.count > ids.size()) throw std::logic_error("trace exceeds multiplicity");
        for (std::size_t j = trace.offset; j < trace.offset + trace.count; ++j) {
            const auto original = p.items[ids[j]].original;
            if (selected[original] != trace.remove) throw std::logic_error("trace repeats an item");
            selected[original] = !trace.remove;
        }
    }
    Solution result;
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (!selected[i]) continue;
        result.selected.push_back(i);
        result.profit += input[i].profit;
        result.weight += input[i].weight;
    }
    if (!validate_solution(input, capacity, result)) throw std::logic_error("invalid reconstructed solution");
    Score actual = 0;
    for (const auto& item : p.items) {
        if (selected[item.original]) actual += item.profit;
        if (item.greedy) actual -= item.profit;
    }
    if (actual != table.entries[best].profit) throw std::logic_error("trace profit mismatch");
    return result;
}

Solution solve(std::span<const Item> input, std::uint64_t capacity, Limits limits,
                 Statistics& stats, bool paper, std::size_t reference_layers = 0) {
    validate_input(input);
    stats = {};
    Solution trivial;
    std::uint64_t eligible = 0;
    std::uint32_t max_weight = 0;
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i].weight > capacity) continue;
        trivial.weight += input[i].weight;
        trivial.profit += input[i].profit;
        trivial.selected.push_back(i);
        ++eligible;
        max_weight = std::max(max_weight, input[i].weight);
    }
    if (trivial.weight <= capacity) return trivial;
    // Section 2.1: in this regime Bellman's O(n*t) is already O(w_max^2).
    if (paper && UnsignedScore{max_weight} > UnsignedScore{eligible} * eligible) {
        stats.capacity_shortcut = true;
        return solve_dp(input, capacity, limits);
    }
    if (limits.max_states == 0 || max_weight > (limits.max_states - 1) / 2)
        throw std::length_error("maximum weight exceeds exchange-state budget");
    auto p = prepare(input, capacity, paper);
    if (reference_layers != 0) {
        p.layer_count = std::min(reference_layers, p.groups.size());
        for (std::size_t g = 0; g < p.groups.size(); ++g) p.groups[g].layer = g % p.layer_count;
    }
    stats.weight_layers = p.layer_count;
    Table table;
    table.entries[0].profit = 0;
    std::vector<Trace> traces;
    if (!paper) {
        resize(table, checked_radius(p, p.radius_cap, limits), stats);
        for (std::size_t g = 0; g < p.groups.size(); ++g) {
            batch_update(table, p, g, false, traces, limits, stats);
            batch_update(table, p, g, true, traces, limits, stats);
        }
        return finish(input, capacity, p, table, traces);
    }
    std::vector<std::size_t> primary;
    std::size_t max_rank = 0;
    for (std::size_t g = 0; g < p.groups.size(); ++g) {
        if (p.groups[g].layer != 0) continue;
        primary.push_back(g);
        max_rank = std::max({max_rank, p.groups[g].add.size(), p.groups[g].remove.size()});
    }
    for (std::size_t f = 0; f < primary.size(); ++f) {
        table.entries[0].positive.push_back(f);
        table.entries[0].negative.push_back(f);
    }
    const auto h = std::bit_width(static_cast<std::uint64_t>(p.max_weight) * 2);
    for (std::size_t band = 1; band <= max_rank; band *= 2) {
        ++stats.rank_phases;
        const auto bound = UnsignedScore{proximity_constant} * p.max_weight *
                           ceil_sqrt(UnsignedScore{2} * band * p.max_weight * h);
        resize(table, checked_radius(p, reference_layers ? UnsignedScore{p.radius_cap} : bound, limits), stats);
        const auto numerator = UnsignedScore{p.max_weight} * h;
        const auto denominator = UnsignedScore{4} * band;
        const auto b_next = reference_layers ? UnsignedScore{primary.size()} :
            std::min(UnsignedScore{primary.size()}, UnsignedScore{proximity_constant} *
                     ceil_sqrt((numerator + denominator - 1) / denominator));
        rank_update(table, p, primary, band - 1, band, static_cast<std::size_t>(b_next), false, traces, limits, stats);
        rank_update(table, p, primary, band - 1, band, static_cast<std::size_t>(b_next), true, traces, limits, stats);
    }
    auto layer_radius = [&](std::size_t layer) {
        if (reference_layers) return checked_radius(p, p.radius_cap, limits);
        const auto numerator = UnsignedScore{4} * proximity_constant * p.max_weight * ceil_sqrt(p.max_weight);
        const auto denominator = UnsignedScore{1} << (layer + 1);
        return checked_radius(p, (numerator + denominator - 1) / denominator + p.max_weight, limits);
    };
    resize(table, std::min(table.radius, layer_radius(0)), stats);
    for (std::size_t layer = 1; layer < p.layer_count; ++layer) {
        for (std::size_t g = 0; g < p.groups.size(); ++g)
            if (p.groups[g].layer == layer) batch_update(table, p, g, false, traces, limits, stats);
        for (std::size_t g = 0; g < p.groups.size(); ++g)
            if (p.groups[g].layer == layer) batch_update(table, p, g, true, traces, limits, stats);
        resize(table, std::min(table.radius, layer_radius(layer)), stats);
    }
    return finish(input, capacity, p, table, traces);
}

} // namespace

Solution solve_partitioned_reference(std::span<const Item> items, std::uint64_t capacity,
                                      std::size_t layers, Statistics& statistics) {
    if (layers == 0) throw std::invalid_argument("reference requires at least one layer");
    return solve(items, capacity, {}, statistics, true, layers);
}

} // namespace knapsack::detail

namespace knapsack {

Solution solve_exchange(std::span<const Item> items, std::uint64_t capacity,
                         Limits limits, Statistics* statistics) {
    Statistics local;
    return detail::solve(items, capacity, limits, statistics ? *statistics : local, false);
}

Solution solve_paper(std::span<const Item> items, std::uint64_t capacity,
                      Limits limits, Statistics* statistics) {
    Statistics local;
    return detail::solve(items, capacity, limits, statistics ? *statistics : local, true);
}

} // namespace knapsack
