#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <numeric>
#include <span>
#include <vector>

namespace knapsack::detail {

// Row maxima of a totally monotone matrix; ties go to the leftmost column.
// The caller supplies a constant-time implicit matrix oracle.
template<class Evaluate>
std::vector<std::size_t> smawk(std::size_t row_count, std::size_t column_count,
                               Evaluate evaluate) {
    std::vector<std::size_t> answer(row_count), rows(row_count), columns(column_count);
    std::iota(rows.begin(), rows.end(), 0);
    std::iota(columns.begin(), columns.end(), 0);
    if (row_count == 0 || column_count == 0) return answer;
    auto recurse = [&](auto&& self, const std::vector<std::size_t>& rs,
                       const std::vector<std::size_t>& cs) -> void {
        if (rs.empty()) return;
        std::vector<std::size_t> reduced;
        reduced.reserve(std::min(rs.size(), cs.size()));
        for (auto c : cs) {
            while (!reduced.empty() &&
                   evaluate(rs[reduced.size() - 1], c) >
                   evaluate(rs[reduced.size() - 1], reduced.back())) reduced.pop_back();
            if (reduced.size() < rs.size()) reduced.push_back(c);
        }
        std::vector<std::size_t> odd;
        for (std::size_t i = 1; i < rs.size(); i += 2) odd.push_back(rs[i]);
        self(self, odd, reduced);
        std::size_t left = 0;
        for (std::size_t i = 0; i < rs.size(); i += 2) {
            std::size_t right = reduced.size() - 1;
            if (i + 1 < rs.size()) {
                right = left;
                while (reduced[right] != answer[rs[i + 1]]) ++right;
            }
            std::size_t best = left;
            for (std::size_t j = left + 1; j <= right; ++j)
                if (evaluate(rs[i], reduced[j]) > evaluate(rs[i], reduced[best])) best = j;
            answer[rs[i]] = reduced[best];
            left = right;
        }
    };
    recurse(recurse, rows, columns);
    return answer;
}

struct MaximumInterval {
    std::size_t begin, end; // Half-open row interval.
    std::size_t column;
};

// floor((rows-1)*sample/(samples-1)) without overflowing either product.
// Callers provide 1 <= samples <= rows and 0 <= sample < samples.
class EvenRowSampler {
    std::size_t gaps_, quotient_, remainder_;
    bool wide_;
public:
    EvenRowSampler(std::size_t rows, std::size_t samples)
        : gaps_(samples - 1), quotient_(gaps_ ? (rows - 1) / gaps_ : 0),
          remainder_(gaps_ ? (rows - 1) % gaps_ : 0),
          wide_(remainder_ && gaps_ > std::numeric_limits<std::size_t>::max() / remainder_) {}

    std::size_t operator()(std::size_t sample) const {
        if (!gaps_) return 0;
        const auto fraction = wide_
            ? static_cast<std::size_t>(static_cast<unsigned __int128>(remainder_) * sample / gaps_)
            : remainder_ * sample / gaps_;
        return quotient_ * sample + fraction;
    }
};

// Appendix A's tall-matrix bound: sample O(c) evenly spaced rows, run
// ordinary SMAWK there, then interpolate only between different maxima.
// Each interpolation level scans O(c) columns in total, for
// O(c * (1 + log(ceil(r/c)))) oracle calls and O(c) space.
template<class Evaluate>
std::vector<MaximumInterval> tall_smawk(std::size_t rows, std::size_t columns,
                                      Evaluate evaluate) {
    std::vector<MaximumInterval> result;
    if (rows == 0 || columns == 0) return result;
    auto emit = [&](std::size_t begin, std::size_t end, std::size_t column) {
        if (begin == end) return;
        if (!result.empty() && result.back().column == column) result.back().end = end;
        else result.push_back({begin, end, column});
    };
    if (columns == 1) { emit(0, rows, 0); return result; }
    const auto samples = std::min(rows, columns);
    const EvenRowSampler sample_row(rows, samples);
    const auto maxima = smawk(samples, columns, [&](auto r, auto c) { return evaluate(sample_row(r), c); });
    auto interpolate = [&](auto&& self, std::size_t first, std::size_t last,
                           std::size_t left, std::size_t right) -> void {
        if (left == right || last - first == 1) { emit(first, last, left); return; }
        const auto middle = first + (last - first) / 2;
        auto best = left;
        for (auto c = left + 1; c <= right; ++c)
            if (evaluate(middle, c) > evaluate(middle, best)) best = c;
        self(self, first, middle, left, best);
        self(self, middle, last, best, right);
    };
    for (std::size_t s = 1; s < samples; ++s)
        interpolate(interpolate, sample_row(s - 1), sample_row(s), maxima[s - 1], maxima[s]);
    emit(rows - 1, rows, maxima.back());
    return result;
}

// Compact tall-Monge maxima via ordered-column crossings. O(c log(r+1))
// oracle calls, O(c) storage, without allocating r rows. This is sufficient
// for the O(L_1 log L) bound in Jin's singleton extension (Lemma 4.1).
template<class Evaluate>
std::vector<MaximumInterval> monge_envelope(std::size_t rows, std::size_t columns,
                                           Evaluate evaluate) {
    std::vector<MaximumInterval> envelope;
    if (rows == 0) return envelope;
    for (std::size_t c = 0; c < columns; ++c) {
        std::size_t start = 0;
        bool dominated = false;
        while (!envelope.empty()) {
            const auto previous = envelope.back();
            if (!(evaluate(rows - 1, c) > evaluate(rows - 1, previous.column))) {
                dominated = true;
                break;
            }
            std::size_t lo = previous.begin, hi = rows - 1;
            while (lo < hi) {
                const auto mid = lo + (hi - lo) / 2;
                if (evaluate(mid, c) > evaluate(mid, previous.column)) hi = mid;
                else lo = mid + 1;
            }
            start = lo;
            if (start == previous.begin) envelope.pop_back();
            else break;
        }
        if (dominated) continue;
        if (envelope.empty()) start = 0;
        else envelope.back().end = start;
        envelope.push_back({start, rows, c});
    }
    return envelope;
}

} // namespace knapsack::detail
