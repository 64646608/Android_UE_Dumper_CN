# DFM 2362 SDK 迁移记录（2026-11-05）

## 目标与版本
- 旧 APK：1.201.37117.67（2207）。
- 新 APK：1.201.37119.49（2362），versionCode=2362，ARM64，`libUE4.so` 已加载。
- 独立 QA 分支：`qa-dfm-ue4-adaptation`；原 Dumper 和 `dfm_gm` 的运行逻辑暂不修改。

## 已取得的诊断证据
```text
QA_DFM NamePool hit_rva=0x10791a80 candidate_rva=0x200f7d00 readable=1
QA_DFM POOL_CHECK ... name0='None' decode_plausible=1
QA_DFM OBJECT_LAYOUT_CANDIDATE rva=0x2011eb48 count=238803 validated_indices=24/24
QA_DFM SUMMARY sig_guobjects=0 namepool_readable=1 name0_plausible=1 structural_objects=1
```

名称池候选和对象数组结构候选具有合理性，但仍只是**候选**。旧版字节特征命中为零，不能直接当作失败或绕过的证据。尚未通过完整的 UObject 名称、类关系、字段布局和 SDK 生成验收。

旧适配源文件 `AndUEDumper/src/UE/UEGameProfiles/DeltaForce.hpp` 使用针对 2207 的固定 RVA；2362 的 `--qa-dfm-dump` 在 NamePool 预检查时返回 `ERROR_INIT_NAMEPOOL`。不要将本记录中的候选 RVA 自动写入旧版运行配置。

## 目前已交付：离线差异检查
`scripts/sdk_layout_diff.py` 仅对两份**已有的** `AIOHeader.hpp` 文件进行文本解析和结构对照，不读写运行中进程、不抓包、不修改 APK。

运行示例：

```bash
python3 scripts/sdk_layout_diff.py \
  --old /path/to/2207/AIOHeader.hpp \
  --new /path/to/2362/AIOHeader.hpp \
  --json /path/to/sdk_diff_2207_2362.json \
  --csv /path/to/sdk_diff_2207_2362.csv
```

工具输出新旧类数量、类大小改变、成员类型/宽度/偏移的改变。它不验证运行时的安全性或真实指针地址。

## 下阶段验收
1. 从**自己拥有授权的构建**、调试符号、已生成的新版 SDK 或源代码中获得 2362 的类布局。
2. 运行离线差异工具并复查重要类字段。
3. 把结果整理成 2362 的**独立配置候选表**，逐项注明依据与验证状态；不要覆盖已验证的 2207 配置。
4. 独立验证坐标字段究竟为 FVector 还是 FEncVector，并核对真实世界坐标。
5. 只有具备充分证据才考虑用于 QA 客户端；当前没有已完成的 2362 SDK 导出物。

备注：保留旧版云编译记录和所有原始证据，避免从结构候选推导为已成功解密或已恢复所有偏移。
