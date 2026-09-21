# 0-1 Knapsack in Nearly Quadratic Time — C++20

Ce Jin 的 [arXiv:2308.04093v2](https://arxiv.org/abs/2308.04093v2) 算法的研究实现。
核心、测试、实例生成器和基准程序均使用 C++20。

目前提供可运行的精确求解器和独立对拍基线。代码包含贪心交换、排名分阶段、hint
传播、两层确定性着色、普通及压缩输出 SMAWK、批量更新和解的回溯。
**本项目采用 `O(n + w_max^2 log^5 w_max)` 的确定性时间上界。**
论文给出的界为 `log^4`；当前实现与该界的差异保留在
[复杂度审计](docs/complexity.md)和独立审查中。

## 构建与测试

需要 CMake 3.20+ 和支持 C++20、`__int128` 的编译器，例如 GCC 或 Clang。
项目不需要第三方运行时库或 Python。`__int128` 是编译器扩展，配置阶段会检查支持情况。

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
./build-release/knapsack_cli --input examples/small.txt --stats
```

示例输出：

```text
profit 11
weight 7
items 2 2 3
```

物品下标从零开始。输入第一行是 `物品数 容量`，随后每行是 `重量 收益`。
默认从标准输入读取。`--stats` 将统计写到标准错误，不混入解的输出。

## 求解器

```sh
./build-release/knapsack_cli --algorithm paper --input examples/small.txt
./build-release/knapsack_cli --algorithm exchange --input examples/small.txt
./build-release/knapsack_cli --algorithm dp --input examples/small.txt
./build-release/knapsack_cli --algorithm brute --input examples/small.txt
```

- `paper`：论文框架及 witness propagation；默认选项。
- `exchange`：同重量批量更新的交换 DP，用于独立对照。
- `dp`：经典容量 DP。
- `brute`：Gray code 子集枚举，最多 25 件物品。

库接口位于 [knapsack.hpp](include/knapsack/knapsack.hpp)。所有求解器返回
`Solution{profit, weight, selected}`，并提供独立的 `validate_solution`。

## 数值与资源范围

重量为正 `uint32_t`，收益为正 `uint64_t`，容量为 `uint64_t`。
输入重量和收益的总和须分别适合 `uint64_t`；论文算法还会检查扰动收益和中间运算的范围。
这些运算使用精确整数。详见 [数值约定](docs/numerics.md)。

默认每张状态表最多 2,000,001 个状态，每个回溯节点池最多 20,000,000 个节点。
CLI 可用 `--max-states` 和 `--max-traces` 调整；库可传入 `Limits`。
它们是结构容量限制，不是进程总内存限制。超限或算术范围不足会抛出异常，CLI 返回状态码 2。

`paper` 在论文 §2.1 指定的 `w_max > n^2` 情形调用容量 DP，统计中
`capacity_shortcut` 会标记该分支。其余分支不会在资源不足时静默更换算法。

## 生成数据与运行实验

```sh
./build-release/knapsack_generate --n 200 --wmax 32 --seed 42 --family correlated > instance.txt
./build-release/knapsack_cli --input instance.txt
./build-release/knapsack_bench --algorithm paper --n 200 --wmax 32 --seed 42 --repeats 3 --verify
./build-release/knapsack_bench --algorithm dp --n 200 --wmax 32 --seed 42 --repeats 3
```

支持 `uniform`、`correlated`、`subset`、`duplicates` 和 `ties` 分布。
`--capacity-percent` 默认为总重量的 50%。`--verify` 额外用容量 DP 核对收益。
`--duplicate-percent P` 将 `floor(n*P/100)` 件物品的重量设为生成上界；其他随机物品也可能有同样重量。
CSV 末尾记录容量比例、强制重复比例及实际最大重量件数。`duplicates` 分布固定为 100%。
实例生成、可选对照求解和输出校验不计入求解计时。

基准输出 CSV。Linux 下 `process_peak_rss_kib` 是整个进程到当前时刻的内存高水位，
包含可选校验和此前重复实验；对比内存时为每个配置分别启动进程且不启用 `--verify`。
其他平台该字段为零。保守的理论常数会影响运行速度，实测结果须与所用配置一起报告。

## 开发验证

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DKNAPSACK_SANITIZERS=ON
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
```

测试包括完整求解器对拍、回溯验证、隐式矩阵与压缩输出、hint 扩展的条件性契约、
并列最优解、确定性着色、不同重量层的衔接、输入错误和资源边界。
阶段安排保存在 [实现计划](docs/implementation-plan.md)。
本机测试范围与三次重复的性能记录见 [验证报告](docs/validation.md)。
核心扩展另有 [矩阵查询计数实验](docs/extension-probes.md)，用于核对源传播及多着色步骤。
完整交付核对见 [验收清单](docs/acceptance-audit.md)，复杂度的独立交叉审查见 [审查记录](docs/independent-review.md)。
