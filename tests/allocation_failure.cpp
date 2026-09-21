#include "knapsack/knapsack.hpp"

#include <array>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace {
constinit std::atomic<std::size_t> allocation_count{0};
constinit std::atomic<std::size_t> live_allocations{0};
thread_local std::ptrdiff_t successful_allocations_left = -1;

void* allocate(std::size_t size) {
    if (successful_allocations_left == 0) throw std::bad_alloc{};
    if (successful_allocations_left > 0) --successful_allocations_left;
    auto* pointer = std::malloc(size == 0 ? 1 : size);
    if (!pointer) throw std::bad_alloc{};
    ++allocation_count;
    ++live_allocations;
    return pointer;
}

void release(void* pointer) noexcept {
    if (!pointer) return;
    --live_allocations;
    std::free(pointer);
}

using Solver = knapsack::Solution (*)(std::span<const knapsack::Item>, std::uint64_t);

void check(const char* name, Solver solve) {
    constexpr std::array<knapsack::Item, 4> items{{{3, 4}, {4, 5}, {2, 3}, {5, 8}}};
    std::size_t successful_call_allocations = 0;
    const auto start = allocation_count.load();
    {
        const auto result = solve(items, 7);
        successful_call_allocations = allocation_count.load() - start;
        if (result.profit != 11 || !knapsack::validate_solution(items, 7, result))
            throw std::runtime_error("unfaulted solver returned an incorrect solution");
    }
    if (successful_call_allocations == 0) throw std::runtime_error("no allocations were exercised");
    std::size_t exceptions = 0;
    for (std::size_t point = 0; point < successful_call_allocations; ++point) {
        const auto before = live_allocations.load();
        successful_allocations_left = static_cast<std::ptrdiff_t>(point);
        try {
            const auto result = solve(items, 7);
            successful_allocations_left = -1;
            // An implementation may recover, but it must still return an exact,
            // feasible solution. Validation itself runs outside fault injection.
            if (result.profit != 11 || !knapsack::validate_solution(items, 7, result))
                throw std::runtime_error("solver returned a partial result after allocation failure");
        } catch (const std::bad_alloc&) {
            successful_allocations_left = -1;
            ++exceptions;
        } catch (...) {
            successful_allocations_left = -1;
            throw;
        }
        if (live_allocations.load() != before) {
            std::cerr << name << ": leak at allocation " << point << '\n';
            throw std::runtime_error("allocation failure leaked owned memory");
        }
    }
    if (exceptions == 0) throw std::runtime_error("allocation failures did not reach the caller");
    std::cout << name << ": checked " << successful_call_allocations << " allocation failure points\n";
}
} // namespace

// The replacement is confined to this test executable. The library uses
// ordinary-alignment standard containers; these hooks never affect production.
void* operator new(std::size_t size) { return allocate(size); }
void* operator new[](std::size_t size) { return allocate(size); }
void operator delete(void* pointer) noexcept { release(pointer); }
void operator delete[](void* pointer) noexcept { release(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { release(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { release(pointer); }

int main() {
    try {
        std::cout << "Allocation failure checks\n";
        check("paper", [](auto items, auto capacity) { return knapsack::solve_paper(items, capacity); });
        check("exchange", [](auto items, auto capacity) { return knapsack::solve_exchange(items, capacity); });
        check("capacity DP", [](auto items, auto capacity) { return knapsack::solve_dp(items, capacity); });
        check("exhaustive oracle", [](auto items, auto capacity) { return knapsack::solve_brute_force(items, capacity); });
    } catch (const std::exception& error) {
        successful_allocations_left = -1;
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
