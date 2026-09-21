#pragma once

#include <algorithm>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <utility>

namespace knapsack::detail {

// Median-of-medians selection: worst-case linear time, including adversarial inputs.
template<class T, class Less>
void select_nth(std::span<T> a, std::size_t nth, Less less) {
    if (nth >= a.size()) throw std::out_of_range("select_nth index");
    while (a.size() > 32) {
        const std::size_t groups = (a.size() + 4) / 5;
        for (std::size_t i = 0; i < groups; ++i) {
            const std::size_t first = 5 * i, last = std::min(first + 5, a.size());
            std::sort(a.begin() + static_cast<std::ptrdiff_t>(first),
                      a.begin() + static_cast<std::ptrdiff_t>(last), less);
            std::swap(a[i], a[first + (last - first) / 2]);
        }
        select_nth(a.first(groups), groups / 2, less);
        const T pivot = a[groups / 2];
        std::size_t first_equal = 0, scan = 0, last_equal = a.size();
        while (scan < last_equal) {
            if (less(a[scan], pivot)) std::swap(a[first_equal++], a[scan++]);
            else if (less(pivot, a[scan])) std::swap(a[scan], a[--last_equal]);
            else ++scan;
        }
        if (nth < first_equal) a = a.first(first_equal);
        else if (nth < last_equal) return;
        else {
            a = a.subspan(last_equal);
            nth -= last_equal;
        }
    }
    std::sort(a.begin(), a.end(), less);
}

} // namespace knapsack::detail
