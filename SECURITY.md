# Security Policy / 安全策略

## Supported Versions / 支持版本

| Version / 版本 | Supported / 支持 |
| --- | --- |
| 2.1.x | ✅ |
| < 2.1 | ❌ |

Only the latest release receives security fixes. Older releases may contain
known issues and should be upgraded before reporting a problem.

仅最新版本会获得安全修复。旧版本可能包含已知问题，报告前请先升级。

## Reporting a Vulnerability / 报告漏洞

**Please do not open a public issue for a security vulnerability.** Public
reports can expose users before a fix is available.

**请勿通过公开 Issue 报告安全漏洞。** 公开报告可能在修复发布前暴露风险。

Use GitHub private vulnerability reporting:

<https://github.com/null07089/Lineage_Hide/security/advisories/new>

If private reporting is unavailable, open a minimal public issue asking for a
private contact channel. Do not include vulnerability details in that issue.

如果无法使用私密报告，可以提交一个不含漏洞细节的公开 Issue，请维护者提供私密
联系方式。

Chinese and English reports are both welcome.

中英文报告均可。

## What to Include / 报告内容

Please include as much of the following as possible:

- Module version, release tag, and installation method
- Device model, Android/ROM version, kernel version, and architecture
- Root manager (Magisk/KernelSU) and Zygisk implementation
- SUSFS status and relevant kernel/module configuration
- Affected component (`service.sh`, native Hook, build script, etc.)
- Reproduction steps, expected behavior, and actual behavior
- Impact assessment and attack prerequisites
- Sanitized logs or a minimal proof of concept

请尽量提供：

- 模块版本、Release 标签和安装方式
- 设备型号、Android/ROM 版本、内核版本和架构
- Root 管理器（Magisk/KernelSU）与 Zygisk 实现
- SUSFS 状态及相关内核/模块配置
- 受影响组件（`service.sh`、native Hook、构建脚本等）
- 复现步骤、预期行为和实际行为
- 影响评估与攻击前提
- 已脱敏的日志或最小化 PoC

## Response / 响应

This is a volunteer-maintained project. We aim to acknowledge reports within
7 days and provide an initial assessment within 30 days, but these are
best-effort targets rather than a service-level guarantee.

本项目由志愿者维护。我们力争在 7 天内确认收到报告、30 天内给出初步评估，但这些
只是尽力而为的目标，不构成服务级别保证。

## Scope / 范围

### In scope / 属于范围

- Vulnerabilities in this repository's code, scripts, and release artifacts
- Memory-safety or logic bugs in the Zygisk/native Hook code
- Command injection, unsafe path handling, symlink attacks, or unsafe file
  permissions in `service.sh`, installation scripts, or packaging scripts
- Unexpected privilege escalation or unintended exposure of root/module
  artifacts caused by the module
- Supply-chain or build-integrity issues in releases produced by this project
- Security issues in the optional kernel patch scripts maintained here

- 本仓库代码、脚本和发布产物中的漏洞
- Zygisk/native Hook 代码中的内存安全或逻辑问题
- `service.sh`、安装脚本或打包脚本中的命令注入、不安全路径处理、符号链接攻击
  或不安全文件权限
- 模块导致的意外提权或 root/模块产物意外暴露
- 本项目发布流程中的供应链或构建完整性问题
- 本仓库维护的可选内核补丁脚本中的安全问题

### Out of scope / 不属于范围

- Detection-bypass failures, anti-cheat results, Play Integrity results, or
  app-specific ROM/root detection. These are functionality issues; use the
  public issue tracker.
- Vulnerabilities in third-party ROMs, kernels, Magisk, KernelSU, Zygisk Next,
  SUSFS, or other modules. Report them to the relevant upstream project.
- Problems caused by unsupported device/ROM/kernel combinations, incorrect
  installation, or ignoring the documented requirements.
- Issues that require an already-rooted shell, physical access, or the ability
  to modify the device's boot image.
- Social engineering, spam, or denial-of-service reports that require root.

- 检测绕过失败、反作弊结果、Play Integrity 结果，或特定应用的 ROM/root 检测。
  这些属于功能问题，请使用公开 Issue。
- 第三方 ROM、内核、Magisk、KernelSU、Zygisk Next、SUSFS 或其他模块自身的漏洞。
  请向对应上游项目报告。
- 不支持的设备/ROM/内核组合、错误安装或忽略文档要求导致的问题。
- 需要已 root shell、物理接触或修改设备启动镜像才能触发的问题。
- 需要 root 权限的社会工程、垃圾信息或拒绝服务报告。

Because this module requires root and an unlocked bootloader, the baseline
threat model already assumes a privileged environment. A report that only
shows an app can detect root, a custom ROM, or the module itself is not a
security vulnerability by itself; it needs to demonstrate an unexpected
privilege boundary crossing or a concrete harm.

由于本模块需要 root 和解锁 bootloader，基础威胁模型已假定存在特权环境。仅仅
证明某个应用能够检测到 root、自定义 ROM 或模块本身，并不构成安全漏洞；报告需要
证明意外的权限边界突破或具体危害。

## Coordinated Disclosure / 协调披露

We prefer coordinated disclosure: please give us a reasonable opportunity to
investigate and release a fix before publishing details. We will credit
reporters who want attribution, unless they prefer to remain anonymous.

我们采用协调披露：请在公开漏洞细节前，给予我们合理的调查和修复时间。除非报告者
希望匿名，否则我们会在致谢中注明报告者。

## Safe Harbor / 安全港

We welcome good-faith security research. If you follow this policy, avoid
accessing other users' data, avoid disrupting services, and do not publicly
disclose details before a fix is available, we will treat your report as a
collaborative security contribution.

我们欢迎善意的安全研究。只要你遵守本策略、不访问其他用户的数据、不破坏服务，
并在修复发布前不公开细节，我们会将你的报告视为协作性的安全贡献。

## Sensitive Data / 敏感数据

Remove or redact personal and sensitive information before submitting logs,
including account names, phone numbers, IMEI/serial numbers, location data,
tokens, keys, and any third-party data.

提交日志前请删除或脱敏个人信息和敏感数据，包括账号、电话号码、IMEI/序列号、
位置信息、令牌、密钥及任何第三方数据。

## No Bug Bounty / 无赏金

This project does not offer a bug bounty program. Security reports are handled
on a best-effort, voluntary basis.

本项目不提供漏洞赏金计划。安全报告由维护者以志愿、尽力而为的方式处理。
