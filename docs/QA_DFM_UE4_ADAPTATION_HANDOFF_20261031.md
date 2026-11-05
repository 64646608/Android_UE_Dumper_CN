# UE4 FPS Dumper 独立适配交接（2026-10-31）
 
## 目标
在只用于私服测试的 Android ARM64 环境中，基于开源 Android_UE_Dumper_CN，独立验证并导出当前版本 UE4 对象结构，为人物、Bot、武器与摄像机调试提供数据基础。暂不开发/修改游戏内存写入功能。

## 仓库与分支
- 仓库：64646608/Android_UE_Dumper_CN（从 DreamFekk/Android_UE_Dumper_CN Fork）
- 主分支：main，保持原样
- 工作分支：qa-dfm-ue4-adaptation
- Actions：.github/workflows/build-arm64-qa.yml
- 原始工程子模块：deps/KittyMemoryEx；Actions 使用 submodules: recursive
- 构建目标：ARM64 / Android API 24 / NDK r26d；UEDump3r_arm64

## 已有证据
- 手机游戏包名：com.tencent.tmgp.dfm（用户修改过包名，不代表内部版本相同）
- 游戏版本：1.201.37117.67，ARM64
- 用户提供的 libUE4.so Build ID：2cdcd1a90870be747f3090993b73383359c17f47
- libUE4.so 已由进程成功加载，基址曾为 0x6ff784f000；该基址仅对当次进程有效。
- 旧版 UE4Dumper --dfmdiag：UObject valid=0/112，停止 Dump；证明旧版配置不匹配，不等于证明内存加密。
- 自制 V0.6 只读扫描：394 名称池形状候选、42 World 形状候选、0 对象数组候选；没有经过 UE 真实对象交叉验证，不可当作正确偏移。
- DreamFekk 的 DeltaForceProfile 提供名字解码和扫描模式，但用户的磁盘版 SO 上旧特征未静态匹配；运行时仍待验证。

## 已做操作
1. 通过 GitHub 连接确认新 Fork 存在且拥有 push 权限。
2. 创建分支 qa-dfm-ue4-adaptation。
3. 增加 GitHub Actions 的 ARM64 NDK 编译流程，使用可复现的 NDK 版本和子模块拉取。
4. Actions #41191463375 首次因过期的 SDK `tools` 依赖而失败；修复 workflow，移除相关 setup action。
5. Actions #41191543294 完整通过，包括 NDK 安装、C++ 编译及 artifact 上传。
6. 产物：UEDump3r_arm64，ELF64 ARM64 PIE；文件 SHA256 `112ac9653e801215801a444bf90a844c085896309351a81b30e350b6fcccd499`。
7. GitHub artifact: https://github.com/64646608/Android_UE_Dumper_CN/actions/runs/41191543294/artifacts/13039305587
8. 真机 Dump 验收尚未完成。

## 尚未完成
- CI 编译已通过，尚未完成目标版本真机兼容性验证。
- 未修改真实 GNames / GUObjectArray / GWorld RVA。
- 没有完成任何真正 SDK Dump。
- 没有开发人物、Bot、武器、摄像机的实时客户端绘制功能。

## 验收顺序
1. Actions 构建成功，产物为 Android AArch64 ELF 可执行程序。
2. Android Root 测试机仅执行 --help / 文件哈希检查。
3. 新增只读游戏布局/对象有效性诊断，并验证名称字符串、对象数量与类型。
4. 验证至少数十个 UObject（类、名称、外部对象关系）无异常，再输出 SDK。
5. 将真实状态接入独立的调试绘制层，双号/人机测试。

## 风险与约束
- 不使用卖家付费启动器、授权流程或私有 payload。
- 不依赖客户端包名判断实际二进制版本。
- 不将候选值伪装为验证过的地址。
- 继续维持 main 不变，所有适配通过独立分支和 PR 评审。
- 对 Android 下载的未知 Root 可执行程序先检查来源与 SHA256。
