# 代码与论文发布记录

## 代码

仓库：[kzoacn/knapsack](https://github.com/kzoacn/knapsack)。
原有 `main` 历史保留，代码、说明、测试和原始实验 CSV 已提交并推送。

- 论文引用的实现快照：[`2d15cc23bd33d0ce1af912f0c3aefab032ae4f3a`](https://github.com/kzoacn/knapsack/tree/2d15cc23bd33d0ce1af912f0c3aefab032ae4f3a)。
- 稿件快照：[`f92580016cd3c9986ac2c25d1bf88ac1b7d3e5ff`](https://github.com/kzoacn/knapsack/tree/f92580016cd3c9986ac2c25d1bf88ac1b7d3e5ff/paper)。
- 从实现提交导出的干净目录重新构建，Release CTest 7/7 通过。
- 同一实现的 AddressSanitizer + UndefinedBehaviorSanitizer CTest 7/7 通过；详情见[验证报告](validation.md)。

## 论文

题目：*A Deterministic C++20 Implementation of Proximity-Based 0-1 Knapsack*。

- 作者：[kzoacn](https://github.com/kzoacn)。
- 语言：英文；分类：`cs.DS`。
- 论文许可证：[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)。
- [Markdown 正文](../paper/paper.md)与[声明元数据](../paper/metadata.json)。
- 采用的实现时间界：`O(n + W^2 log^5(2W))`，并明确区分原论文的 `log^4` 定理。
- 披露 OpenAI / GPT-6 / Codex 辅助及独立子代理审查；未暴露的模型信息记为 unknown。

## Markdownxiv 投稿

正式入口：[markdownxiv.github.io](https://markdownxiv.github.io/)。
投稿使用当前 `agent-preprints-v6` 协议及 `v6-2026-09-21` 生产挑战。
本地工作量证明与 `common-isotropic-v1` 数学证书均已通过平台验证器。

- 投稿：[PR #21](https://github.com/Markdownxiv/markdownxiv.github.io/pull/21)。
- 投稿提交：`eb8636c80fccd8caa8eb511cf0e8bdb932b31102`。
- 状态：2026-09-21 已接收、归档并发布，回执为 `accepted`、`archived: true`、`published: true`。
- 论文编号：[mx:2609.00010v1](https://markdownxiv.github.io/abs/2609.00010v1/)。
- [在线正文](https://markdownxiv.github.io/md/2609.00010v1/)与[原始 Markdown](https://markdownxiv.github.io/md/2609.00010v1.md)。
- [投稿流水线](https://github.com/Markdownxiv/markdownxiv.github.io/actions/runs/35596817914)。

已下载线上原始 Markdown，与本地稿件逐字节比较一致。两者的 SHA-256：

```text
f42901a61e42432362469fe5b7b219b887eeb6dc7b498d1734ca5da69a61876f
```

平台内容承诺：

```text
1a54d0e9b43ec0faec43e546e48845bafdd645a2b3771c8a45bf5dd97f69b8a4
```

平台接收及发布不等同于外部同行评审或对论文结论的认证。
