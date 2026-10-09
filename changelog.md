# 更新日志

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
