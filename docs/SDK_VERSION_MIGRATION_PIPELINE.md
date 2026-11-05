# DFM SDK 一键版本迁移（阶段化，不覆盖旧配置）

## 为什么不能保证“任意新 APK 自动导出 SDK”
一个 APK 可以带有不同版本、裁剪的符号和不同的 UE 结构。对于未提供新版调试符号/SDK 的构建，**只凭 APK 文件名或旧偏移，不能可靠生成类成员的最新版地址**。缺乏依据时应标记 BLOCKED，而不是填入猜测偏移。

## 阶段 A：Android 手机一键采集版本信息
- 下载本分支 `scripts/android_apk_inventory.sh`，放到 `/data/local/tmp/`
- 安装新 APK 后运行：

```sh
su -c 'sh /data/local/tmp/android_apk_inventory.sh com.tencent.tmgp.dfm'
```

- 脚本记录 APK 版本、SHA256、可读取的 on-disk `libUE4.so` 哈希和已存在的 SDK 导出文件位置。它不访问游戏进程内存、不生成 SDK、不修改网络。
- 在 `/sdcard/Download/` 生成 `dfm_inventory_*.txt`，可发送给维护者核对版本。

## 阶段 B：提供与该构建匹配的 SDK
优先使用自有的 UE4 源代码构建、调试符号或经授权导出的 `AIOHeader.hpp` / SDK ZIP。检查来源、版本、Build ID 与可识别类数量。

## 阶段 C：一键离线比较
已有工具 `scripts/sdk_layout_diff.py`：

```sh
python3 scripts/sdk_layout_diff.py --old old/AIOHeader.hpp --new new/AIOHeader.hpp --json sdk_diff.json --csv sdk_diff.csv
```

可对比类大小、字段类型、字节偏移、位字段掩码；不会连接任何运行中的游戏。

## 阶段 D：检查门槛与构建
1. 无新版 SDK → BLOCKED_NEED_NEW_SDK；不修改 `conf.hpp`。
2. 无旧 SDK → BLOCKED_NEED_OLD_SDK。
3. 新 SDK 数量显著减少或无法解析 → BLOCKED_NEW_SDK_INCOMPLETE。
4. 有版本匹配且结构对比完成 → REVIEW_REQUIRED（还不是 PASS）。
5. 在自有测试构建中核对全局符号、坐标字段类型、对象身份和矩阵后，才能创建独立的版本配置和 CI 构建。

## 历史版本
- 2207：原有 SDK/配置保留，不覆盖。
- 2362：当前只有 NamePool 和 GUObjectArray 结构候选，没有完整的 2362 SDK。
- 不把结构候选写死为新的真实全局变量地址。

## 交付说明
本次同时提供聊天可下载的 `dfm_sdk_migration_kit.zip`，包括手机脚本和一键离线对比运行器（支持直接读取 SDK ZIP/ APK），及自检。公共代码仓库仅保存不含游戏专有数据的脚本和说明。原有 `dfm_gm` 的程序和偏移没有改动。
