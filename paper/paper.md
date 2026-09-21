# A Deterministic C++20 Implementation of Proximity-Based 0-1 Knapsack

**Author:** [kzoacn](https://github.com/kzoacn)  
**Date:** 21 September 2026  
**Article type:** Implementation and reproducibility report

## Abstract

We present an exact C++20 implementation of the proximity-based framework for
0-1 knapsack developed by Ce Jin. The artifact includes greedy-boundary selection,
rank and weight stages, deterministic coloring, hinted witness propagation,
implicit matrix search, and recovery of original item indices. A direct accounting
of the implemented control flow gives a conservative deterministic running-time
bound of $O(n+W^2\log^5(2W))$, where $n$ is the input size and $W$ is the maximum
eligible item weight, under the stated integer-arithmetic and resource assumptions.
The additional logarithmic factor distinguishes this implementation guarantee from
Jin's $O(n+W^2\log^4 W)$ theorem. We explain why propagated hint handles cannot
automatically be charged to the initial source count, and provide executable
component checks and an implementation-specific audit. Exact integer arithmetic,
explicit resource limits, independent solvers, and allocation-failure tests support
reproducibility. Release and sanitizer configurations each pass all seven CTest
tests. Small experiments report both timing and process memory, including cases
where the simpler exchange-DP baseline is faster. The contribution is a documented
research implementation and its audit, rather than a new asymptotic knapsack
algorithm.

## 1. Scope and problem definition

Given positive integer weights $w_i$, positive integer profits $p_i$, and a
nonnegative integer capacity $t$, the problem is

$$
\max_{X\subseteq\{1,\ldots,n\}}
\sum_{i\in X}p_i
\quad\text{subject to}\quad
\sum_{i\in X}w_i\le t.
$$

The implementation returns the optimum profit, total selected weight, and
zero-based indices in the original input. Items heavier than $t$ are ineligible.
Write $n_e$ for the number of eligible items and $W$ for their maximum weight;
an empty eligible set is handled directly. We use
$H=\max\{1,\lceil\log_2(2W)\rceil\}$ when stating bounds.

Jin's deterministic algorithm has an $O(n+W^2\log^4 W)$ bound and combines
proximity, witness propagation, color coding, and matrix searching
([Jin, 2024](https://arxiv.org/abs/2308.04093v2)). Its structural background includes
dense subset sums ([Bringmann and Wellnitz, 2020](https://arxiv.org/abs/2010.09096))
and fine-grained proximity for bounded knapsack
([Chen et al., 2023](https://arxiv.org/abs/2307.12582v2)).

This report documents one concrete realization of that framework. Its audited
upper bound is $O(n+W^2H^5)$. Neither the tests nor the complexity audit establish
that the fifth logarithmic factor is necessary for the full solver. The artifact
also provides three comparison solvers: classical capacity DP, subset enumeration,
and a simpler exchange DP. The bound in this report applies to `solve_paper`,
including its explicitly identified capacity-DP branch.

## 2. Artifact and algorithm organization

The code and supporting audit are available in
[kzoacn/knapsack](https://github.com/kzoacn/knapsack). The implementation snapshot
described here is commit
[`2d15cc23bd33d0ce1af912f0c3aefab032ae4f3a`](https://github.com/kzoacn/knapsack/tree/2d15cc23bd33d0ce1af912f0c3aefab032ae4f3a).
All solver code, tests, instance generators, and benchmark executables are C++20.
GCC or Clang support for the `__int128` extension is required; CMake checks this
requirement at configuration time. The solver has no third-party library or
Python runtime dependency.

| Component | Main implementation | Role |
| --- | --- | --- |
| Input preparation and selection | `src/selection.hpp`, `src/solver.cpp` | Find the greedy boundary and retain boundary candidates |
| Implicit matrix search | `src/monge.hpp` | Ordinary SMAWK and compressed tall-matrix output |
| Hinted extension | `src/hinted.cpp` | Propagate source handles and recover count witnesses |
| Deterministic coloring | `src/coloring.cpp` | Isolate small hints and partition large hints |
| Stages and item recovery | `src/solver.cpp` | Compose rank and weight updates and reconstruct indices |
| Independent baselines | `src/baseline.cpp` | Capacity DP and exhaustive subset enumeration |

### 2.1. Boundary candidates

The implementation uses deterministic median-of-medians selection to find the
efficiency boundary without sorting the entire input. After separating the greedy
prefix from the remaining items, it groups items by weight. For each weight, it
retains at most $2W$ candidate additions and $2W$ candidate removals. Linear
selection precedes sorting of the retained items. Addition lists are ordered by
decreasing profit; removal lists are ordered by increasing profit.

The state index represents a signed weight change from the greedy solution.
Same-weight prefix sums provide the concave functions used for batch updates.
Rank blocks have sizes $1,2,4,\ldots$; addition and removal passes use opposite
index orientations. Persistent trace nodes record a predecessor, weight group,
rank offset, count, and addition/removal direction. They avoid copying a full
selected-item set into every state.

### 2.2. Hints and conditional optimality

A hint is a set of weight functions associated with an input state. An extension
entry stores an immutable source handle, so a newly reached position refers to
the appropriate original hint without copying it. Processing successive colors
propagates that handle along with the selected witness.

The component contract is conditional. At a target position, equality with the
unrestricted extension optimum is required when **every** unrestricted optimal
witness uses only functions permitted by its own source hint. All returned
witnesses must be feasible and respect their source hints. Existence of just one
hint-compatible optimum does not establish the universal premise. The component
oracle therefore enumerates all tied optima before deciding whether equality is
required. The full solver relies on the structural stage invariants to retain an
optimal solution through the sequence of such conditional extensions.

The implementation's correctness argument thus separates feasible witness
construction from structural preservation of an optimum. Recovery follows the
stored counts back to the retained item lists, applies additions and removals to
the greedy selection, and independently checks the resulting original indices.
Randomized test generation supplements this argument; it is not a formal
verification of the implementation or of its mathematical dependencies.

### 2.3. Finite multiplicities and compressed matrices

Each extension carries its actual physical count limit. Empty weight groups do
not create sources. For a fixed function and residue, the search clips the row
range to physically reachable positions and separates source blocks whose
reachable intervals have a gap. Each output arithmetic progression ends at its
source's count limit.

For an implicit totally monotone matrix with $r$ rows and $c$ columns, the tall
search samples at most $c$ rows and interpolates between their maximizing columns.
Intervals with the same maximizing column at both ends are emitted without
enumerating their rows. At each interpolation depth, the total candidate-column
span is $O(c)$, yielding

$$
O\!\left(c\left[1+\log\left\lceil r/c\right\rceil\right]\right)
$$

oracle accesses in the tall case. If each source permits at most $M$ copies,
the split blocks have at most $c(M+1)$ rows, giving
$O(c[1+\log(M+1)])$ accesses per block. These reductions improve concrete work
without requiring a change to the global bound claimed here.

## 3. Conservative complexity accounting

The model counts operations on checked, fixed-width integers as constant time.
It applies to supported inputs with sufficient configured state and trace limits.
It is not a bit-complexity bound for unbounded-precision profits. Define all
logarithms below with a lower cutoff of one.

### 3.1. The source-count issue

Let $N$ be the table length, $b$ the maximum hint size, and $A$ the number of
currently usable sources in one singleton extension. The directly justified
cost is $O(N+A\log N)$. A handle can spread from one input state to many output
positions, so handle sharing does not bound the next call's number of matrix
columns by the number of initial sources. A regression example in the repository
starts with one reachable state and a two-function hint; after composition, a
later singleton call can have all 100 table positions available as sources.

The repository's
[independent audit](https://github.com/kzoacn/knapsack/blob/2d15cc23bd33d0ce1af912f0c3aefab032ae4f3a/docs/independent-review.md)
goes further: it gives an explicit family for the current generic extension and
tall-search control flow with $b=2$ and $\Omega(N\log^2 N)$ matrix accesses.
The construction is accompanied by an executable multistar probe and a recursive
query-count argument. Its multiplicities do not satisfy the production solver's
per-weight candidate restriction, so it is not a lower bound for `solve_paper`.
It also does not disprove Jin's existence theorem. Its role here is to prevent an
unsupported use of the stronger Lemma 4.8 accounting for this concrete generic
subroutine. The cross-check was performed by a separate AI agent and reviewed in
the implementation session; it was not external peer review.

### 3.2. Small and general hints

For small hints, the deterministic isolating family uses $O(\log N)$ colorings
and at most $b^2$ colors per coloring. In production calls the function universe
has size at most $W=O(N)$. Construction and incidence handling are bounded by
$O(Nb^3+|U|\log N)$. Charging each singleton call using its current source count,
which can be $N$, gives

$$
T_{\mathrm{small}}(N,b)
=O\!\left(Nb^3+Nb^2\log^2 N\right).
$$

For $b$ larger than a constant multiple of $\log N$, the outer deterministic
balancing partitions the functions into $O(b/\log N)$ groups. Every restricted
hint then has size $O(\log N)$. Substitution in the preceding expression, including
the integer balancing and handle operations, gives

$$
T_{\mathrm{extend}}(N,b)
=O\!\left(Nb\log^3(Nb)\right).
$$

The same general upper bound covers the small-hint branch, with empty hints
handled directly. This calculation deliberately does not substitute initial
source counts for current ones.

### 3.3. Summing the stages

The retained rank lists contain at most $2W$ items per weight and side, so there
are $O(H)$ rank stages. With $N_j$ the table length and $b_j$ the support bound at
stage $j$, the adopted structural radii give

$$
N_jb_j=O(W^2H),\qquad \log(N_jb_j)=O(H).
$$

These parameter products follow from the inverse square-root support bound and
square-root growth of the rank radius. Integer ceilings, the first-stage actual
support, and physical radius clipping are included in the
[implementation audit](https://github.com/kzoacn/knapsack/blob/2d15cc23bd33d0ce1af912f0c3aefab032ae4f3a/docs/complexity.md).
Each positive or negative extension therefore takes $O(W^2H^4)$ time.
Summing $O(H)$ stages gives $O(W^2H^5)$.

Preparation costs $O(n+W^2H)$: the full input is scanned linearly and only the
retained lists are sorted. Transferring recovered count witnesses to stage traces
costs $O(N_jb_j)$ per stage. In the later weight layers, the number of newly
processed groups grows geometrically while the layer radius decreases
geometrically; their batch-update products are lower-order terms. Reconstructing
and checking the final selection costs $O(n)$.

The explicit branch $W>n_e^2$ uses capacity DP. A nontrivial instance has
$t<n_eW$, hence $n_et<n_e^2W<W^2$. Input handling remains $O(n)$.
Combining the branches yields the implementation bound

$$
\boxed{T(n,W)=O(n+W^2H^5).}
$$

The derivation uses structural proximity results, together with the repository's
conservative constant calculations. It does not derive a sharper bound from the
finite timing measurements.

## 4. Exact arithmetic and engineering limits

For eligible items indexed $i=1,\ldots,n_e$, the implementation uses

$$
B=1+n_e+\frac{n_e(n_e+1)}2,\qquad
\widetilde p_i=(Bp_i+i)W+1.
$$

The scale makes any difference in original integral profit dominate the total
secondary perturbation. The code uses the perturbed values for ordering and
intermediate optimization and reports original profit values. Efficiency
comparisons use integer cross-products, rather than floating-point ratios.

Positive intermediate values are restricted to the checked $2^{124}$ range;
unreachable states have a separate $-2^{126}$ sentinel. With total perturbed
profit $P$ and exchange radius $L$, allocation is preceded by a check of
$(P+1)(8L+16)\le 2^{124}$. Input aggregate weight and original profit must each
fit `uint64_t`. Unsupported ranges produce exceptions, rather than approximate
answers or modular arithmetic. The tall-row sampler uses quotient/remainder
decomposition and a wider intermediate when a `size_t` product could overflow.

The deterministic coloring also uses integers. Its balancing potential rounds
division upward and budgets the accumulated rounding error. A final per-set
discrepancy certificate checks the promised split. Integer square roots are used
for structural thresholds.

The production proximity constant is $2^{25}$, and radii are capped by the
independent $2W^2$ bound and available total weight. The accompanying
[numerical notes](https://github.com/kzoacn/knapsack/blob/2d15cc23bd33d0ce1af912f0c3aefab032ae4f3a/docs/numerics.md)
and [uniform-multiplicity derivation](https://github.com/kzoacn/knapsack/blob/2d15cc23bd33d0ce1af912f0c3aefab032ae4f3a/docs/uniform-bound.md)
record the conservative constants used to instantiate the structural arguments.
They are not thresholds fitted to the benchmarks. Their size often leaves only
one production weight layer at experimentally accessible scales. Dedicated tests
inject several layers using a safe global radius to exercise the interfaces.

By default, each state table is limited to 2,000,001 entries and each trace pool to
20,000,000 nodes. These are per-structure limits, not a process-memory cap.
Allocation failures and explicit limit violations are reported to the caller;
the CLI returns status 2. No arbitrary-precision implementation or MSVC support
is claimed.

## 5. Validation

On the recorded environment, both Release and AddressSanitizer plus
UndefinedBehaviorSanitizer configurations passed all seven CTest tests. A fresh
build exported from the implementation commit also passed all seven tests,
checking that the tracked repository is sufficient to build the artifact.

| Validation layer | Recorded coverage |
| --- | --- |
| Full solver comparisons | 1,000 small random instances against enumeration and capacity DP; 40 larger instances against capacity DP |
| Implicit matrix search | 500 matrix comparisons against row enumeration; a billion-row compressed case; large sampler indices without huge allocations |
| Conditional hinted extension | 700 exhaustive-oracle cases, including ties; half include physical count limits |
| Coloring and stage composition | 200 isolating-family checks, nontrivial balancing, and 10 safe multi-layer integration cases |
| Failure handling | 475 injected allocation-failure positions across the four public solvers for one fixture |
| Interfaces and contracts | Invalid input, arithmetic/resource boundaries, witness recovery, graph-hint and multistar probes |

The allocation-failure counts are 269 for `paper`, 195 for `exchange`, 9 for
capacity DP, and 2 for enumeration. These checks cover all ordinary allocation
positions observed for that fixture, not all possible allocation patterns of all
inputs. The independent solution validator checks selected-index uniqueness,
capacity, and reported aggregates. Exact profit agreement with independent
baselines provides a separate optimality check on the tested instances.

## 6. Experimental results

### 6.1. Method

Measurements were recorded on 21 September 2026 using an AMD Ryzen 7 8845H,
WSL2 Linux 6.18.33.2, GCC 13.3.0, and CMake 4.4.2. Solver runs were single-threaded
Release builds. Each algorithm/configuration used a separate process with three
repetitions. Reported times are medians in milliseconds; input generation and
solution validation are outside the timed region. Builds and sanitizer tests
were not run concurrently with these measurements.

The base grid has $n\in\{200,1000\}$ and a configured maximum weight
$W\in\{16,32,64\}$. The C++ generator uses `std::mt19937_64` with seed 42,
weights `1 + rng() % W`, and profits `1 + rng() % (4*W)` for its `uniform` family.
Thus the generator uses modulo sampling, without a claim of perfect discrete
uniformity. Capacity is half the total item weight, rounded down. All three
solvers and all repetitions agree on optimum profit.

### 6.2. Timing and memory

| $n$ | $W$ | Optimum profit | `paper` (ms) | `exchange` (ms) | `dp` (ms) |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 200 | 16 | 5,291 | 2.726 | 1.396 | 2.127 |
| 200 | 32 | 10,440 | 17.117 | 7.570 | 4.182 |
| 200 | 64 | 22,047 | 59.799 | 31.368 | 8.033 |
| 1,000 | 16 | 26,655 | 3.266 | 2.150 | 63.886 |
| 1,000 | 32 | 52,579 | 28.073 | 16.506 | 129.104 |
| 1,000 | 64 | 107,910 | 190.945 | 108.974 | 273.795 |

The exchange baseline is faster than `paper` in all six measured configurations.
Capacity DP is faster than `paper` in the three $n=200$ configurations; `paper`
is faster in the three $n=1000$ configurations. These are observations about this
small grid, not a general ranking or evidence for an asymptotic exponent.

For $n=1000,W=64$, the measured process RSS high-water marks are 57,524 KiB
(`paper`), 47,264 KiB (`exchange`), and 266,400 KiB (`dp`). RSS includes the
input, live state and trace storage, allocator effects, and earlier repetitions
in the same process. The benchmark's optional `--verify` mode was disabled for
these memory comparisons because it first runs capacity DP in that process.
The DP baseline reconstructs selected items; these memory measurements are not
a lower bound for a value-only capacity-DP implementation.

For the same $n=1000,W=64$ instance, development measurements before finite-count
clipping and source-block splitting recorded 28,317,252 matrix queries and a
354.338 ms median. The optimized measurements record 8,879,398 queries and
190.945 ms. This is an operation-count and timing comparison on one instance;
it does not imply a change to the asymptotic bound. The archived performance
snapshot precedes the final large-index sampler overflow fix. That fix preserves
the sampling indices used at these small benchmark dimensions, but the times
were not remeasured afterward.

The 54 raw base measurements and the historical snapshot are included in
[`docs/benchmark-results.csv`](https://github.com/kzoacn/knapsack/blob/2d15cc23bd33d0ce1af912f0c3aefab032ae4f3a/docs/benchmark-results.csv)
and
[`docs/benchmark-results-initial.csv`](https://github.com/kzoacn/knapsack/blob/2d15cc23bd33d0ce1af912f0c3aefab032ae4f3a/docs/benchmark-results-initial.csv).

### 6.3. Capacity and repeated-weight coverage

A second grid fixes $n=200,W=32$ and seed 42. It crosses capacities of 10%, 50%,
and 90% of total weight with nine configurations: five `uniform` cases forcing
0%, 25%, 50%, 75%, or 100% of weights to $W$, plus `correlated`, `subset`, `ties`,
and `duplicates` families. Forced positions are distributed through the input;
the remaining randomly generated weights can also equal $W$.

The resulting 27 configurations, three solvers, and three repetitions produce
243 rows in
[`docs/distribution-results.csv`](https://github.com/kzoacn/knapsack/blob/2d15cc23bd33d0ce1af912f0c3aefab032ae4f3a/docs/distribution-results.csv).
Profits agree throughout. This grid broadens tested input shapes, but its single
seed and small size do not constitute a large performance study.

## 7. Reproduction

Build the fixed implementation snapshot and run the complete Release suite:

```sh
git clone https://github.com/kzoacn/knapsack.git
cd knapsack
git checkout 2d15cc23bd33d0ce1af912f0c3aefab032ae4f3a
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j 4
ctest --test-dir build-release --output-on-failure
```

Run the example and one benchmark process per algorithm:

```sh
./build-release/knapsack_cli --input examples/small.txt --stats
for algorithm in paper exchange dp; do
  ./build-release/knapsack_bench --algorithm "$algorithm" \
    --n 1000 --wmax 64 --seed 42 --repeats 3
done
./build-release/knapsack_multistar_probe 64
```

The example returns profit 11 and weight 7 with selected indices 2 and 3.
Use `--verify` for an additional capacity-DP profit check, with the memory-accounting
caveat above. The README documents input syntax, all distributions, CLI limits,
and sanitizer configuration. The public library interface is
`include/knapsack/knapsack.hpp`. The current benchmark appends distribution
parameters after the original 15 CSV columns; older base measurements retain
their original schema.

## 8. Limitations and further work

The supported numeric domain is finite, and large proximity constants limit the
practical scales and number of production weight layers tested. The generic
hinted extension has a conservative accounting gap relative to the stronger
target used in the original paper. Recovering a $\log^4 W$ implementation bound
would require a valid additional structural amortization or a different extension
algorithm, together with its correctness argument. Merely sharing hint handles
does not provide that accounting.

The experiments cover one machine and a small set of reproducible distributions.
They do not compare against mature general-purpose knapsack packages, establish
practical superiority, or determine crossover points reliably. Formal verification,
tighter constants, broader performance evaluation, and lower-overhead recovery
remain useful directions. The present artifact provides concrete code, exact
test oracles, raw measurements, and an explicit implementation guarantee for
such follow-up work.

## AI assistance and review provenance

OpenAI's GPT-6 model family, accessed through the Codex client, assisted with
implementation, testing, complexity analysis, and manuscript preparation. An
independent Codex subagent cross-checked the generic extension analysis. Exact
runtime model versions and the subagent's exact model identity were not exposed
and are declared unknown. These are
provenance declarations, not authenticated model identities. This report has not
undergone external peer review; Markdownxiv admission does not certify its
mathematical claims or the quality of the implementation.

## References

1. Ce Jin. *0-1 Knapsack in Nearly Quadratic Time*. arXiv:2308.04093v2, 2024.
   [Version used for implementation](https://arxiv.org/abs/2308.04093v2).
2. Karl Bringmann and Philip Wellnitz. *On Near-Linear-Time Algorithms for Dense
   Subset Sum*. arXiv:2010.09096, 2020.
   [Full version](https://arxiv.org/abs/2010.09096).
3. Lin Chen, Jiayi Lian, Yuchen Mao, and Guochuan Zhang. *Faster Algorithms for
   Bounded Knapsack and Bounded Subset Sum Via Fine-Grained Proximity Results*.
   arXiv:2307.12582v2, 2023.
   [Full version](https://arxiv.org/abs/2307.12582v2).

## License

This manuscript is licensed under
[Creative Commons Attribution 4.0 International (CC BY 4.0)](https://creativecommons.org/licenses/by/4.0/).
