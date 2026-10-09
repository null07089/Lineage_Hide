# Lineage Hide

Lineage Hide is a target-scoped Zygisk module that removes LineageOS and other
custom-ROM fingerprints from selected applications.  It was split from the
Yukari project and re-implemented as version 1.0 with a new module id, boot
companions and documentation.

## Pairing with SUSFS (important)

The module only changes what a target *process* can observe.  What the kernel
reports — path visibility, already-open file descriptors, system properties —
is covered by SUSFS on a KernelSU kernel, and Lineage Hide ships the glue for
it:

- `service.sh` runs on every boot and uses the bundled `ksu_susfs` binary to
  register ROM-named files with `add_sus_path`, their mappings and idmaps with
  `add_sus_map`, and to delete or scrub lineage system properties with
  `resetprop`;
- it also hides the module's own `zygisk/arm64-v8a.so` and the Zygisk library
  from application `/proc/self/maps`;
- `/proc/<pid>/fd/*` readlink targets inherited from zygote need the kernel
  patch from `scripts/patch-kernel.sh`; SUSFS alone does not rewrite them.

Without SUSFS the module still hides the five in-process channels below, but
native file and property scanners can still fingerprint the ROM.

## Hidden channels

| Channel | Mechanism | Switch |
| --- | --- | --- |
| ServiceManager services | `BinderProxy.transactNative` JNI hook; enumeration and debug replies are filtered in the Parcel layer, direct app lookups are rewritten in a private request copy. Keywords: `lineage`, `crdroid`, `aospa`, `pixelexperience`, `omnirom`, `protonaosp`, plus the exact name `profile` | always on |
| Resource package | AssetManager name/ID lookups treat `lineageos.platform` (resource package id `0x3f`) as absent | `hide_lineage_resources` |
| System features | `hasSystemFeature` requests are rewritten and the Parcel read path returns equal-length placeholders for the eight `org.lineageos.*` names | `hide_lineage_features` |
| Protected broadcasts | the ten lineage protected actions are replaced in a private copy of outbound `IActivityManager` requests | `hide_lineage_broadcasts` |
| Reflection constant | `AssetManager.LINEAGE_APK_PATH` is removed from `getDeclaredField`, `getField` and the `getDeclaredFields` family | always on |

All in-process channels are length-preserving, process-local and never write
into Binder reply buffers.

## Configuration

`/data/adb/modules/lineage_hide/config.json`:

```json
{
  "enabled": true,
  "force_denylist_unmount": true,
  "hide_lineage_resources": true,
  "hide_lineage_features": true,
  "hide_lineage_broadcasts": true,
  "targets": ["com.example.app"]
}
```

- `enabled` — master switch; the module does nothing when `false`.
- `force_denylist_unmount` — pass `FORCE_DENYLIST_UNMOUNT` to Zygisk.  Set to
  `false` on devices where target apps depend on Magisk-provided mounts.
- `hide_lineage_resources` — hide the `lineageos.platform` resource package
  from target processes (Android 9+ entry points).  Apps that legitimately
  consume Lineage SDK resources lose them inside that process.
- `hide_lineage_features` — hide the `org.lineageos.*` system features.
- `hide_lineage_broadcasts` — rewrite lineage protected-broadcast actions so
  sending them behaves like AOSP instead of raising `SecurityException`.
- `targets` — package names the module applies to.  Protected packages and
  system processes are always skipped.

`module/action.sh` (installed as
`/data/adb/modules/lineage_hide/action.sh`) manages the target list
interactively; a timeout or unavailable input preserves the existing file.

## Boot integration (`service.sh`)

After the boot animation starts, the module:

1. deletes system properties whose name contains `lineage` and rewrites values
   that contain `lineage`;
2. walks `/system`, `/vendor`, `/system_ext` and `/product` for `*lineage*` and
   `*gapps*` entries and registers them with `ksu_susfs add_sus_path`
   (`add_sus_map` for regular files);
3. registers additional paths and maps: `addon.d`, the LineageOS update
   directory, SELinux policy files, the platform resource RRO and its idmaps,
   the module's own native library and the Zygisk library.

Every registration is idempotent and safe to repeat on the next boot.

## Kernel-side path hiding

`scripts/patch-kernel.sh` applies the procfs complement to a kernel source
tree: for application UIDs (>= 10000) the path strings printed by
`/proc/<pid>/fd/*`, `/proc/<pid>/map_files/*`, `/proc/<pid>/maps`, `smaps` and
`numa_maps` are rewritten so they never contain the ROM keywords.  The patcher
is semantic (it locates functions, not line numbers) and idempotent, so trees
with vendor or SUSFS modifications work as well:

```bash
scripts/patch-kernel.sh /path/to/kernel          # apply
scripts/patch-kernel.sh --check /path/to/kernel  # status only
scripts/patch-kernel.sh --revert /path/to/kernel # remove
```

It writes `include/linux/lineage_hide.h`, edits `do_proc_readlink()` in
`fs/proc/base.c` and `seq_file_path()` in `fs/seq_file.c`, and backs up edited
files as `*.lineage_hide.bak`.  Rebuild and flash the kernel afterwards; only
the displayed string changes, file access and resource loading are untouched.

## Build

Install Gradle 8.11.1, JDK 17 and the Android SDK/NDK.  The repository's
`gradlew` delegates to Gradle on `PATH`.

```bash
./gradlew :module:assembleRelease
bash scripts/package.sh
```

`v*` tags are built by GitHub Actions, which publishes `Lineage_Hide.zip` and
updates the root `update.json` on `main`, so Magisk/KernelSU managers offer
module updates from the repository's releases.

## Verification

Quick checks; the detailed checklist with commands is in
[README.zh-CN.md](README.zh-CN.md).

- In a target process: `getSystemAvailableFeatures()` shows no
  `org.lineageos.*` names, the `lineageos.platform` resource id `0x3f` is
  absent, and `AssetManager.class.getDeclaredField("LINEAGE_APK_PATH")` throws
  `NoSuchFieldException`.
- In a non-target process all of the above must still be visible.
- `logcat -s LineageHide` shows the hook installation lines; `service.sh`
  output appears in the kernel/manager log.

## Boundaries

- Zygisk keeps the module `.so` mapped in zygote.  Updates require a reboot,
  and replacing the file under a live mapping can crash processes.
- `PendingIntent`-relayed broadcasts, native-only Binder lookups and native
  reflection are outside the current scope.
- Without SUSFS and the kernel patch, native scanners can still observe ROM
  files, mappings and properties from application processes.
