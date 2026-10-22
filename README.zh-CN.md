# Lineage Hide

[English](README.md) | [简体中文](README.zh-CN.md)

Lineage Hide 是一个 Zygisk 模块，用于隐藏 LineageOS / 自定义 ROM 指纹。
**全部三方应用生效**

> **English**: Lineage Hide is a Zygisk module that hides LineageOS / custom ROM
> fingerprints from third-party apps. The complete English documentation is in
> [README.md](README.md).

## 目录

- [安装](#安装)
- [兼容性](#兼容性)
- [演示](#演示)
- [与 SUSFS 搭配（重要）](#与-susfs-搭配重要)
- [Zygisk Next 重要配置（否则部分应用会闪退）](#zygisk-next-重要配置否则部分应用会闪退)
- [隐藏通道总览](#隐藏通道总览)
- [注入范围（无配置）](#注入范围无配置)
- [启动集成（service.sh）](#启动集成servicesh)
- [内核侧路径隐藏](#内核侧路径隐藏)
- [KernelSU SELinux 状态加固](#kernelsu-selinux-状态加固)
- [构建与发布](#构建与发布)
- [分阶段验证](#分阶段验证)
- [设计约束与经验](#设计约束与经验)
- [目录结构](#目录结构)
- [许可证](#许可证)

## 安装

1. 在 Magisk / KernelSU 管理器中刷入 `Lineage_Hide.zip` 并重启设备；
2. 需要可用的 Zygisk 实现（KernelSU 上为 Zygisk Next），并按下文
   「Zygisk Next 重要配置」设置三项开关；
3. 完整隐藏还需要内核侧 SUSFS 与可选的内核补丁，见下文；
4. 更新模块后必须重启；版本变更见 [changelog.md](changelog.md)。

## 兼容性

- 理论上支持 LineageOS 23.2 及以上版本；
- 测试通过：OnePlus 8T，LineageOS 24（Android 17）；
- 安装模块后如果暴露 `/system/bin/service` 或 `/system/bin/pm` 挂载点，需要
  安装元模块（MetaModule）：
  <https://kernelsu.org/guide/metamodule.html>；
- 一加欧版机型请安装官方“软件商店”，否则部分应用的特征检查会命中：
  <https://www.oppomobile.com/m/store.html>；
- **部分软件无法打开**：请改用这个 Zygisk Next 版本
  <https://t.me/c/2022002916/124739>
  （新版 Zygisk 注入实现在 360 加壳应用上存在自旋问题），并按
  「Zygisk Next 重要配置」设置管理器；

## 演示

本项目隐藏效果演示视频：<https://b23.tv/437BHCi>

<p align="center">
  <img src="images/native-detector.png" width="280" alt="Native Detector：环境正常，未检测到修改">
  <img src="images/duck-detector.png" width="280" alt="Duck Detector：无高危项">
</p>
<p align="center">
  <img src="images/play-integrity.png" width="280" alt="Play Integrity：BASIC、DEVICE、STRONG 全部通过">
  <img src="images/system-update.png" width="280" alt="系统更新：LineageOS 24 更新界面在系统应用中正常显示">
</p>

抖音登录成功示例（OnePlus 8T）：

<p align="center">
  <img src="images/douyin-login.jpg" width="280" alt="抖音：登录成功，进入欢迎页">
  <img src="images/douyin-device-management.png" width="280" alt="抖音：登录设备管理显示 OnePlus 8T 本机">
</p>

## 与 SUSFS 搭配（重要）

模块只能改变“目标进程内能看到什么”。内核对外报告的内容——路径可见性、已经打开
的 fd、系统属性——依赖 KernelSU 内核上的 SUSFS，本项目内置配套胶水：

- `service.sh` 每次开机调用系统提供的 `ksu_susfs`：
  - 开启 `hide_sus_mnts_for_non_su_procs` 与 `enable_avc_log_spoofing`；
  - 把带 ROM 关键词的文件注册为 `add_sus_path`，使目录列表与路径查询对应用不可见；
  - 把文件映射与 idmap 注册为 `add_sus_map`，使其从应用 `/proc/self/maps` 中消失；
  - 用 `resetprop` 删除或清洗含 `lineage` 的系统属性；
  - 隐藏模块自身的 `zygisk/arm64-v8a.so` 与 Zygisk 库；
- 从 zygote 继承的 fd 的 readlink 目标（`/proc/<pid>/fd/*`）需要
  `scripts/patch-kernel.sh` 打的内核补丁；SUSFS 单独不会改写它。

没有 SUSFS 时，模块仍能隐藏下面五类进程内信号，但 native 文件/属性扫描仍可能
识别出 ROM。

## Zygisk Next 重要配置（否则部分应用会闪退）

<p align="center">
  <img src="images/zygisk-next-settings.png" width="320" alt="Zygisk Next 设置：Denylist Policy 为 Unmount Only、Use anonymous memory 关闭、Use Zygisk Next linker 开启">
</p>

Zygisk Next 管理器里请按上图设置，否则部分应用（尤其是 360 加壳应用）会在启动时闪退：

- **Denylist Policy** 选 `Unmount Only`，不要用 `Enforce DenyList`；
- **Use anonymous memory** 关闭；
- **Use Zygisk Next linker** 开启。

模块通过 `FORCE_DENYLIST_UNMOUNT` 在应用进程内卸载挂载点，这三项设置需要与之配合。
Zygisk Next 版本选择见上方“兼容性”一节。

## 隐藏通道总览

| 通道 | 机制 |
| --- | --- |
| ServiceManager 服务 | `BinderProxy.transactNative` JNI hook；枚举/调试回复在 Parcel 层过滤；应用发起的直接 lookup 在私有请求副本中等长改写。关键词：`lineage`、`crdroid`、`aospa`、`pixelexperience`、`omnirom`、`protonaosp`，精确匹配 `profile` |
| 资源包 | AssetManager 名称/ID 查询把 `lineageos.platform`（资源包 id `0x3f`）按不存在处理 |
| 系统 feature | 改写 `hasSystemFeature` 请求；`Parcel` 读取侧把 8 个 `org.lineageos.*` 名称替换为等长占位串 |
| 受保护广播 | 复制外发 `IActivityManager` 广播请求并等长替换 10 个 lineage Action |
| 反射常量 | `AssetManager.LINEAGE_APK_PATH` 从 `getDeclaredField`/`getField`/`getDeclaredFields` 系列中移除（仅对 `AssetManager` 生效） |

所有进程内通道都**常开**、只作用于被注入的应用进程、保持长度不变，且不写 Binder
回复缓冲区。

## 注入范围（无配置）

模块在满足以下条件时注入一个进程：

1. uid 属于应用范围（`>= 10000`）；
2. 其包名不在系统包清单中。

`service.sh` 每次开机会用 `pm list packages -s` 生成系统包清单
（`/data/adb/modules/lineage_hide/system_packages.txt`），因此**只有三方应用**
会被注入；清单生成前（刚安装后的首次开机）有一段硬编码回退保护，覆盖
SystemUI、Settings、权限控制器、核心 providers、核心服务与 `org.lineageos.*`
应用。`FORCE_DENYLIST_UNMOUNT` 始终开启。

如需放过某个三方应用，把它的包名临时加进 `system_packages.txt` 即可（下次开机会
被重新生成覆盖）。

## 启动集成（service.sh）

开机动画启动后，模块会：

1. 隐藏 `pm` 命令探测：把 `/system/bin/pm` 备份为 `/data/system/pm_`，wrapper
   写到 `/data/system/pm`，再用 `nsenter -m -t 1` 在 init mount namespace 里
   bind mount 到 `/system/bin/pm`；wrapper 把调用转给 `cmd package`，对应用
   进程（uid ≥ 10000）的输出做等长 `lineage` → `lineagx`、
   `Lineage` → `Lineagx` 改写；
2. 隐藏 service 列表：
   - OnePlus 8T（`ro.product.model=KB2000`）把 `/` 重挂为可写，备份
     `/system/bin/service` 为 `service_`，写入一个对 `service list` 返回
     392 项定制静态列表的 wrapper；
   - 其余机型（或 8T 写入失败时）不改系统分区：把原文件备份到
     `/data/system/service_`，wrapper 写到 `/data/system/service`，再用
     `nsenter -m -t 1` 在 init mount namespace 里 bind mount 到
     `/system/bin/service`；
   - 8T wrapper 对非 `list` 调用、通用 wrapper 对所有输出做等长
     `lineage` → `lineagx` 改写；
3. 把 `/system`、`/product/etc`、`/system_ext/etc`、
   `/system/system_dlkm/etc` 的 `build.prop` 中 `userdebug` 改写为 `user`，
   不再暴露调试构建信息；
4. 伪装 bootloader / verified boot 状态：`ro.boot.flash.locked=1`、
   `ro.boot.verifiedbootstate=green`、`ro.boot.vbmeta.device_state=locked`、
   `ro.boot.vbmeta.*`，以及 `ro.debuggable=0`、`ro.force.debuggable=0`、
   `ro.secure=1`、`ro.adb.secure=1`、`ro.build.type=user`、
   `ro.build.tags=release-keys`、recovery 状态的 `ro.bootmode` 与
   `init.svc.adb_root`，并把 `ro.config.ringtone` 改写为 `unknown`；
5. 删除名称含 `lineage` 的系统属性，改写值中含 `lineage` 的属性；
6. 禁用 `org.lineageos.jelly`，生成系统包清单（供注入范围判断使用）；
7. 开启 SUSFS 的 `hide_sus_mnts_for_non_su_procs` 与
   `enable_avc_log_spoofing`，然后遍历 `/data/resource-cache`、`/system`、
   `/vendor`、`/system_ext`、`/product` 中带 `*lineage*` 和 `*gapps*`
   的条目，交给 `ksu_susfs add_sus_path`（普通文件同时 `add_sus_map`）；
8. 追加注册：`addon.d`、LineageOS 更新目录、SELinux 策略文件、平台资源包的
   RRO 与 idmap、模块自身 native 库与 Zygisk 库。

`/system/lib64/libstagefright.so` 默认不再注册为 `sus_path`，避免隐藏核心系统库
带来未知副作用；确需隐藏时去掉 `service.sh` 第 511 行注释。

`pm` 与 service 的 wrapper 只对应用进程（uid ≥ 10000）去特征，root/shell 仍得到
原始输出。service 列表步骤只有 OnePlus 8T 会重挂并写入系统分区；通用 bind mount
分支不修改系统分区。第 3 步会重挂对应分区并在完成后挂回只读，无法重挂时文件改写
不生效，但第 4 步的属性伪装仍能覆盖应用运行时读到的值。所有步骤都是幂等的，
重启重复执行没有副作用。与进程内钩子不同，属性改写和 SUSFS 注册是全局生效的。

## 内核侧路径隐藏

`scripts/patch-kernel.sh` 会把 procfs 侧的补丁打到任意内核源码树：对应用进程
（uid ≥ 10000），`/proc/<pid>/fd/*`、`map_files/*`、`maps`、`smaps`、
`numa_maps` 输出的路径字符串不再包含 ROM 关键词。脚本按函数语义定位（不依赖
行号）、幂等，兼容带 vendor/SUSFS 改动的树：

```bash
scripts/patch-kernel.sh /path/to/kernel          # 应用
scripts/patch-kernel.sh --check /path/to/kernel  # 仅报告状态
scripts/patch-kernel.sh --revert /path/to/kernel # 撤销
```

脚本写入 `include/linux/lineage_hide.h`，修改 `fs/proc/base.c` 的
`do_proc_readlink()` 与 `fs/seq_file.c` 的 `seq_file_path()`，修改过的文件备份
为 `*.lineage_hide.bak`。应用后需重新编译并刷入内核；补丁只改显示字符串，不影响
文件访问与资源加载。

## KernelSU SELinux 状态加固

`scripts/selinux_hide_patch.sh` 加固 KernelSU 的 `selinux_hide` 特性：该特性
代替当前（被 KernelSU 修改过的）策略，用开机时留存的策略副本来回答应用的
selinuxfs 查询。在特性开启的前提下，脚本在 `kernel/feature/selinux_hide.c`
中加三处守卫，使这些回退路径对应用进程（uid ≥ 10000）直接返回 `-EINVAL`，
不再验证以下特权上下文：

- `my_write_context()`（`/sys/fs/selinux/context`）拒绝 `u:r:adbroot:s0`；
- `my_write_access()`（`/sys/fs/selinux/access`）拒绝 `u:r:shell:s0` +
  `u:r:su:s0`、`u:r:fsck_untrusted:s0`、`u:r:adbroot:s0`；
- `ksu_handle_selinux_setprocattr()` 拒绝 `u:r:adbroot:s0`。

环境检测应用常借这三类探针确认净化后的策略背后是否藏着 `su`、`adbroot` 等
特权域；拒绝后无论内核实际运行哪份策略，探针都无法得到肯定结果。root/shell
进程（uid < 10000）不受影响。

与 `patch-kernel.sh` 相同，脚本按函数语义定位（不依赖行号）且幂等：

```bash
scripts/selinux_hide_patch.sh /path/to/kernel          # 应用
scripts/selinux_hide_patch.sh --check /path/to/kernel  # 仅报告状态
scripts/selinux_hide_patch.sh --revert /path/to/kernel # 撤销
```

`<tree>` 为内核源码根目录（KernelSU 克隆在 `KernelSU/` 并软链为
`drivers/kernelsu`）或 KernelSU 仓库根目录。修改后的
`kernel/feature/selinux_hide.c` 备份为 `*.selinux_hide.bak`；应用后需重新编译
并刷入内核。

## 构建与发布

安装 Gradle 8.11.1、JDK 17 与 Android SDK/NDK；仓库内的 `gradlew` 使用 `PATH`
上的 Gradle。

```bash
./gradlew :module:assembleRelease
bash scripts/package.sh
```

`v*` 标签由 GitHub Actions 构建，发布 `Lineage_Hide.zip` 并把根目录
`update.json` 同步到 `main`，因此 Magisk/KernelSU 管理器可以从 Release 检查更新。

## 分阶段验证

1. **模块加载**：三方应用启动后 `logcat -s LineageHide` 应出现
   `enabled uid=<uid> process=<进程名>`；系统应用（Settings、SystemUI、
   `org.lineageos.*`）不应出现该行；
2. **ServiceManager**：三方应用内枚举服务列表不应出现关键词服务；
   Settings 等系统应用仍能看到；
3. **资源包**：三方应用内
   `getIdentifier("config_enableLiveDisplay", "bool", "lineageos.platform")`
   返回 `0`；系统应用仍返回真实 ID；
4. **feature**：三方应用内
   `hasSystemFeature("org.lineageos.livedisplay")` 为 `false`，
   `getSystemAvailableFeatures()` 无真实 `org.lineageos.*` 名称；
5. **广播**：三方应用发送
   `lineageos.intent.action.REFRESH_PREFERENCE` 不抛异常，日志出现
   `scrubbed N lineage broadcast action(s)`；
6. **反射常量**：三方应用内
   `AssetManager.class.getDeclaredField("LINEAGE_APK_PATH")` 抛
   `NoSuchFieldException`；系统应用仍可见；
7. **SUSFS 注册**：`ksu_susfs` 的注册结果可在管理器/SUSFS 状态中查看；
   应用 `/proc/self/maps` 不应出现模块 `.so` 与 lineage 路径；
8. **内核补丁**：应用 `/proc/self/fd/*` 的 readlink 目标含关键词时显示为
   等长改写（如 `lineagx`）；root 进程不受影响；
9. **服务列表**：OnePlus 8T 上 `service list` 返回 392 项静态列表；其他机型
   不应出现含 `lineage` 的服务名；root/shell 仍为原始输出；
10. **pm 命令**：应用内 `pm list packages` 输出不含 `lineage`（显示为
    `lineagx`）；root/shell 中仍为原始输出；
11. **Bootloader 属性**：`getprop ro.boot.flash.locked` 为 `1`，
    `ro.boot.verifiedbootstate` 为 `green`，`ro.boot.vbmeta.device_state`
    为 `locked`，`ro.build.type` 为 `user`，`ro.debuggable` 为 `0`，
    `ro.config.ringtone` 为 `unknown`；
12. **KSU 特权探针**：开启 `selinux_hide` 后，应用内经
    `/sys/fs/selinux/context`、`/sys/fs/selinux/access` 或 setprocattr 探测
    `u:r:adbroot:s0`、`u:r:shell:s0`→`u:r:su:s0` 均返回 `EINVAL`；
    root/shell 进程不受影响；
13. **抖音**：抖音可正常启动并通过环境检测（见上方演示截图）。

## 设计约束与经验

- **不要写 Binder 回复 Parcel。** 回复可能是指向 binder 缓冲区的 data-reference；
  早期实现在回复里原地改写导致多应用 `Parcel::writeInt32` SIGSEGV。现在 feature
  枚举在读取侧替换，广播/服务改写私有请求副本；
- **不要 close 资源系统持有的 fd。** 从 zygote 继承的 overlay/platform-res fd 归
  `ZipArchive` 所有，外部 `close()` 触发 fdsan 直接 SIGABRT；这类 fd 的路径暴露
  由内核补丁负责；
- **更新模块必须重启。** zygote 常驻映射模块 `.so`，热替换正在映射的文件会让
  新旧页混用并导致 zygote 崩溃；
- **native 边界。** 进程内钩子只覆盖 Java/框架路径；native 直读 libc 的扫描、纯
  native Binder 调用、`PendingIntent` 代发广播不在覆盖内，需要 SUSFS 与内核补丁
  补齐；
- **隐藏本身要尽量小。** 每多一个钩子就多一份可观测面；已移除对 native 无效的
  list0/readlink 钩子，只保留确有作用的通道；
- **不要隐藏核心系统库。** `/system/lib64/libstagefright.so` 的 `sus_path` 默认
  关闭，隐藏核心系统库可能带来未知副作用；确认无影响后再打开 `service.sh`
  第 511 行；
- **OnePlus 8T 服务列表是静态快照。** ROM 更新改变服务集合后，需要重新生成
  wrapper 才能同步。

## 目录结构

```
module/                      Magisk/KernelSU 模块
  module.prop                模块标识（id=lineage_hide）
  service.sh                 开机：pm/service 去特征 + bootloader 属性伪装 + 系统包清单 + ksu_susfs 注册
  customize.sh               安装脚本（权限）
  src/main/cpp/              Zygisk native 代码
    entry.cpp                注入判定（uid + 系统包清单）与 specialization 入口
    binder_hook.{h,cpp}      ServiceManager BinderProxy JNI 钩子与过滤
    service_filter.{h,cpp}   服务名匹配
    service_cache.{h,cpp}    sCache 清理
    resource_hook.{h,cpp}    资源包隐藏
    feature_filter.{h,cpp}   系统 feature
    broadcast_filter.{h,cpp} 受保护广播
    reflection_filter.{h,cpp} LINEAGE_APK_PATH 反射
    log.{h,cpp}              日志
scripts/
  package.sh                 打包 Lineage_Hide.zip
  patch-kernel.sh            内核 procfs 路径清洗补丁
  selinux_hide_patch.sh      KernelSU selinux_hide 特权上下文探针防护
  verify_stealth.sh          ELF 符号与 /proc maps 辅助验证（开发用）
images/                      README 截图（演示、配置）
```

## 许可证

Copyright (C) 2026 GitHub@null07089。

本项目以 GNU General Public License v3.0 发布，详见 [LICENSE](LICENSE)。
