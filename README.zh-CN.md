# Lineage Hide

Lineage Hide 是一个 Zygisk 模块，用于隐藏 LineageOS / 自定义 ROM 指纹。
**全部三方应用生效**

## 兼容性

- **部分软件无法打开**：请改用这个 Zygisk Next 版本
  <https://t.me/c/2022002916/124739>
  （新版 Zygisk 注入实现在 360 加壳应用上存在自旋问题）

## 演示

本项目隐藏效果演示视频：<https://b23.tv/437BHCi>

## 与 SUSFS 搭配（重要）

模块只能改变“目标进程内能看到什么”。内核对外报告的内容——路径可见性、已经打开
的 fd、系统属性——依赖 KernelSU 内核上的 SUSFS，本项目内置配套胶水：

- `service.sh` 每次开机调用系统提供的 `ksu_susfs`：
  - 把带 ROM 关键词的文件注册为 `add_sus_path`，使目录列表与路径查询对应用不可见；
  - 把文件映射与 idmap 注册为 `add_sus_map`，使其从应用 `/proc/self/maps` 中消失；
  - 用 `resetprop` 删除或清洗含 `lineage` 的系统属性；
  - 隐藏模块自身的 `zygisk/arm64-v8a.so` 与 Zygisk 库；
- 从 zygote 继承的 fd 的 readlink 目标（`/proc/<pid>/fd/*`）需要
  `scripts/patch-kernel.sh` 打的内核补丁；SUSFS 单独不会改写它。

没有 SUSFS 时，模块仍能隐藏下面五类进程内信号，但 native 文件/属性扫描仍可能
识别出 ROM。

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

1. 删除名称含 `lineage` 的系统属性，改写值中含 `lineage` 的属性；
2. 生成系统包清单（供注入范围判断使用）；
3. 遍历 `/system`、`/vendor`、`/system_ext`、`/product` 中带 `*lineage*` 和
   `*gapps*` 的条目，交给 `ksu_susfs add_sus_path`（普通文件同时
   `add_sus_map`）；
4. 追加注册：`addon.d`、LineageOS 更新目录、SELinux 策略文件、平台资源包的
   RRO 与 idmap、模块自身 native 库与 Zygisk 库。

所有步骤都是幂等的，重启重复执行没有副作用。

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
   等长改写（如 `lineagx`）；root 进程不受影响。

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
  list0/readlink 钩子，只保留确有作用的通道。

## 目录结构

```
module/                      Magisk/KernelSU 模块
  module.prop                模块标识（id=lineage_hide，version=1.2）
  service.sh                 开机：属性清理 + 系统包清单 + ksu_susfs 注册
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
  verify_stealth.sh          adb 辅助验证
```
