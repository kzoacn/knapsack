#include "hinted.hpp"
#include "coloring.hpp"
#include "monge.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <numeric>
#include <stdexcept>
#include <tuple>

namespace knapsack::detail {

std::vector<ExtensionEntry> extend_singletons(
    std::span<const ExtensionEntry> input,
    std::span<const std::size_t> singleton,
    std::span<const ConcaveFunction> functions,
    std::vector<ExtensionNode>& nodes, Limits limits, Statistics& statistics) {
    if (input.size() != singleton.size()) throw std::invalid_argument("singleton shape mismatch");
    if (input.size() > limits.max_states) throw std::length_error("singleton table exceeds max_states");
    ++statistics.singleton_calls;
    statistics.peak_states = std::max(statistics.peak_states, input.size());
    struct Source { std::size_t function, residue, row; };
    std::vector<Source> sources;
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i].profit == unreachable || singleton[i] == no_node) continue;
        const auto f = singleton[i];
        if (f >= functions.size() || functions[f].weight == 0)
            throw std::invalid_argument("invalid singleton function");
        if (functions[f].max_count == 0) continue;
        sources.push_back({f, i % functions[f].weight, i});
    }
    statistics.peak_singleton_sources = std::max(statistics.peak_singleton_sources, sources.size());
    // Stable byte-radix grouping preserves the already increasing row order.
    // Fixed-width keys avoid a comparison-sort factor for dense source tables.
    std::vector<Source> scratch(sources.size());
    for (int field = 0; field < 2 && !sources.empty(); ++field) {
        for (std::size_t shift = 0; shift < sizeof(std::size_t) * 8; shift += 8) {
            std::array<std::size_t, 256> counts{};
            auto byte = [&](const Source& s) { return ((field == 0 ? s.residue : s.function) >> shift) & 255; };
            for (const auto& s : sources) ++counts[byte(s)];
            std::size_t offset = 0;
            for (auto& count : counts) { const auto old = count; count = offset; offset += old; }
            for (const auto& s : sources) scratch[counts[byte(s)]++] = s;
            sources.swap(scratch);
        }
    }
    struct Progression { std::size_t source, function, end; };
    struct Event { std::size_t progression, next; };
    std::vector<Progression> progressions;
    std::vector<Event> events;
    std::vector<std::size_t> buckets(input.size(), no_node);
    auto insert = [&](std::size_t row, std::size_t progression) {
        events.push_back({progression, buckets[row]});
        buckets[row] = events.size() - 1;
    };
    for (std::size_t first = 0; first < sources.size();) {
        std::size_t last = first + 1;
        const auto f = sources[first].function;
        const auto w = functions[f].weight;
        while (last < sources.size() && sources[last].function == f &&
               sources[last].residue == sources[first].residue) {
            // Independent physical intervals need not share a matrix. Within
            // each block, r <= c*(max_count+1), hence the search costs
            // O(c*(1+log(max_count+1))) instead of O(c*log(table_length)).
            const auto gap = (sources[last].row - sources[last - 1].row) / w;
            if (gap - 1 > functions[f].max_count) break;
            ++last;
        }
        const auto first_row = sources[first].row, last_source = sources[last - 1].row;
        const auto steps = std::min(functions[f].max_count, (input.size() - 1 - last_source) / w);
        const auto last_row = last_source + steps * w;
        const auto row_count = (last_row - first_row) / w + 1;
        auto evaluate = [&](std::size_t r, std::size_t c) -> Score {
            ++statistics.matrix_queries;
            const auto target = first_row + r * w, source = sources[first + c].row;
            if (target < source) return unreachable;
            return input[source].profit + functions[f]((target - source) / w);
        };
        const auto envelope = tall_smawk(row_count, last - first, evaluate);
        for (const auto& interval : envelope) {
            const auto source = sources[first + interval.column].row;
            auto end = first_row + (interval.end - 1) * w;
            if (source > end || w > end - source) continue;
            const auto allowed = std::min(functions[f].max_count, (end - source) / w);
            end = source + allowed * w;
            const auto begin = std::max(first_row + interval.begin * w, source + w);
            if (begin > end) continue;
            progressions.push_back({source, f, end});
            insert(begin, progressions.size() - 1);
        }
        first = last;
    }
    statistics.candidate_intervals += progressions.size();
    std::vector<ExtensionEntry> result(input.begin(), input.end());
    for (std::size_t i = 0; i < input.size(); ++i) {
        auto winner = no_node;
        Score best = unreachable;
        for (auto event = buckets[i]; event != no_node; event = events[event].next) {
            ++statistics.bucket_visits;
            const auto candidate = events[event].progression;
            const auto& ap = progressions[candidate];
            const auto count = (i - ap.source) / functions[ap.function].weight;
            const Score value = input[ap.source].profit + functions[ap.function](count);
            if (winner == no_node || value > best) { best = value; winner = candidate; }
        }
        if (winner == no_node) continue;
        const auto& ap = progressions[winner];
        const auto w = functions[ap.function].weight;
        if (best > result[i].profit) {
            if (nodes.size() >= limits.max_trace_nodes)
                throw std::length_error("extension exceeds max_trace_nodes");
            nodes.push_back({input[ap.source].head, ap.function, (i - ap.source) / w});
            result[i] = {best, input[ap.source].origin, nodes.size() - 1};
        }
        // Algorithm 1, line 21: propagate the bucket winner even when q[i] wins.
        if (w <= ap.end - i) insert(i + w, winner);
    }
    statistics.trace_nodes = std::max(statistics.trace_nodes, nodes.size());
    return result;
}

ExtensionResult extend_hinted(std::span<const Score> input, const HintSets& hints,
                              std::span<const ConcaveFunction> functions,
                              Limits limits, Statistics& statistics) {
    if (input.size() != hints.size()) throw std::invalid_argument("hinted table shape mismatch");
    if (input.size() > limits.max_states) throw std::length_error("hinted table exceeds max_states");
    ExtensionResult output;
    output.entries.resize(input.size());
    std::size_t bound = 0;
    for (std::size_t i = 0; i < input.size(); ++i) {
        output.entries[i] = {input[i], i, no_node};
        bound = std::max(bound, hints[i].size());
    }
    if (input.empty() || bound == 0) return output;

    // Hints are immutable; an entry's origin is the handle of its hint set.
    // A singleton update changes that handle by composition, not by copying sets.
    auto small = [&](const std::vector<ExtensionEntry>& start, const HintSets& local_hints,
                     const std::vector<std::size_t>& function_ids) {
        auto family = isolating_family(local_hints, function_ids.size());
        statistics.colorings += family.size();
        std::vector<ExtensionEntry> best(input.size());
        std::vector<std::size_t> single_by_handle(input.size(), no_node), singles(input.size());
        for (const auto& coloring : family) {
            std::vector<bool> assigned(input.size(), false);
            std::vector<std::vector<std::pair<std::size_t, std::size_t>>> by_color(coloring.color_count);
            for (auto handle : coloring.isolated) {
                assigned[handle] = true;
                for (auto local_function : local_hints[handle])
                    by_color[coloring.colors[local_function]].emplace_back(handle, function_ids[local_function]);
            }
            auto current = start;
            for (auto& entry : current)
                if (entry.profit != unreachable && !assigned[entry.origin]) entry.profit = unreachable;
            for (const auto& incidence : by_color) {
                if (incidence.empty()) continue;
                for (const auto& [handle, f] : incidence) single_by_handle[handle] = f;
                for (std::size_t i = 0; i < current.size(); ++i)
                    singles[i] = current[i].profit == unreachable ? no_node : single_by_handle[current[i].origin];
                current = extend_singletons(current, singles, functions, output.nodes, limits, statistics);
                for (const auto& [handle, f] : incidence) {
                    (void)f;
                    single_by_handle[handle] = no_node;
                }
            }
            for (std::size_t i = 0; i < best.size(); ++i)
                if (current[i].profit > best[i].profit) best[i] = current[i];
        }
        return best;
    };

    if (bound == 1) {
        std::vector<std::size_t> singles(hints.size(), no_node);
        for (std::size_t i = 0; i < hints.size(); ++i)
            if (!hints[i].empty()) singles[i] = hints[i][0];
        output.entries = extend_singletons(output.entries, singles, functions, output.nodes, limits, statistics);
        return output;
    }
    const auto logarithm = std::max<std::size_t>(1, std::bit_width(2 * input.size()));
    if (bound <= 2 * logarithm) {
        std::vector<std::size_t> ids(functions.size());
        std::iota(ids.begin(), ids.end(), 0);
        output.entries = small(output.entries, hints, ids);
        return output;
    }

    const auto color_count = std::bit_floor(bound / logarithm);
    const auto colors = balanced_coloring(hints, functions.size(), color_count);
    ++statistics.colorings;
    std::vector<std::vector<std::size_t>> members(color_count);
    std::vector<std::size_t> local_index(functions.size());
    for (std::size_t f = 0; f < functions.size(); ++f) {
        local_index[f] = members[colors[f]].size();
        members[colors[f]].push_back(f);
    }
    std::vector<std::vector<std::pair<std::size_t, std::size_t>>> incidence(color_count);
    for (std::size_t i = 0; i < hints.size(); ++i)
        for (auto f : hints[i]) incidence[colors.at(f)].emplace_back(i, local_index[f]);
    for (std::size_t c = 0; c < color_count; ++c) {
        if (incidence[c].empty()) continue;
        HintSets local_hints(input.size());
        for (const auto& [i, f] : incidence[c]) local_hints[i].push_back(f);
        output.entries = small(output.entries, local_hints, members[c]);
    }
    return output;
}

} // namespace knapsack::detail
