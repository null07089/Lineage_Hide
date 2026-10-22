# 更新日志

## 2.0

- 新增抖音过检：`service.sh` 包装 `pm` 命令，应用进程内输出中的 `lineage` 等长改写为 `lineagx`，并把 `ro.config.ringtone` 改写为 `unknown`
- `pm`/`service` 的去特征只对 uid ≥ 10000 的应用进程生效，root/shell 保持原始输出
- 取消注册 `/system/lib64/libstagefright.so` 的 `sus_path`，避免隐藏核心系统库带来未知副作用；需要隐藏时去掉 `service.sh` 第 511 行注释
- 一加欧版机型请安装官方“软件商店”以通过特征检查：<https://www.oppomobile.com/m/store.html>
- 修复 OnePlus 8T 升级后 service wrapper 不刷新，以及通用分支 root 调用路径错误

## 1.5.1

- `service.sh` 统一改用 `/system/bin/sh` 执行，避免部分环境下解释器不一致
- 调整 `service.sh` 执行顺序与语法风格；`pm` 命令前等待 `sys.boot_completed`
- 修复 OnePlus 8T 已有 `service_` 时误走通用挂载分支的问题
- `ro.boot.vbmeta.avb_version` 更新为 `1.4`，适配 Android 17

## 1.5

- `service.sh` 服务列表隐藏改为双方案：OnePlus 8T（`ro.product.model=KB2000`）写入定制静态 `service list` 输出；其余机型把 wrapper 从 `/data/system` bind mount 到 `/system/bin/service`，不再要求系统分区可写
- 新增 bootloader / verified boot 属性伪装：`ro.boot.flash.locked`、`ro.boot.verifiedbootstate`、`ro.boot.vbmeta.*`、`ro.debuggable`、`ro.secure`、`ro.build.type`、`ro.build.tags` 等
- `service.sh` 开启 SUSFS 的 `hide_sus_mnts_for_non_su_procs` 与 `enable_avc_log_spoofing`，并把 `/data/resource-cache` 纳入 lineage/gapps 路径扫描
- 禁用 `org.lineageos.jelly`
- 修复去除 debug 构建信息失败

## 1.4

- `service.sh` 隐藏 service 列表痕迹：包装 `/system/bin/service`，输出中的 lineage 关键词被等长改写
- `service.sh` 清理各分区 `build.prop` 中的 `userdebug` 构建信息

## 1.3

- 作者署名更新为 GitHub@null07089
- `service.sh` 改用系统提供的 `ksu_susfs`，不再随模块分发二进制
- 注册流程加入 3 秒缓冲；README 精简

## 1.2

- 移除全部配置逻辑：无 `config.json`、无目标列表、无开关
- 自动注入所有三方应用（uid ≥ 10000 且不在系统包清单内）
- `service.sh` 每次开机生成系统包清单

## 1.1

- 移除 legacy ioctl 回退，Binder 过滤只走 `BinderProxy` JNI 路径
- 反射过滤限定在 `AssetManager`；Parcel 字符串读取加入长度预筛
- 广播扫描窗口收窄到 4KB；`service.sh` 等待加超时；CI actions 固定到 commit SHA

## 1.0

- 首个独立版本：ServiceManager 服务、`lineageos.platform` 资源包、`org.lineageos.*` feature、受保护广播、`LINEAGE_APK_PATH` 反射隐藏
- 与 SUSFS 搭配：开机注册 sus_path/sus_map 并清理 lineage 系统属性
- 内核侧 procfs 路径清洗脚本（`scripts/patch-kernel.sh`）
