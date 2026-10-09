# Lineage Hide

Lineage Hide is a Zygisk module that removes LineageOS and other custom-ROM
fingerprints from applications.  Every third-party app is covered

## Pairing with SUSFS (important)

The module only changes what a target *process* can observe.  What the kernel
reports — path visibility, already-open file descriptors, system properties —
is covered by SUSFS on a KernelSU kernel, and Lineage Hide ships the glue for
it:

- `service.sh` runs on every boot and uses the system-provided `ksu_susfs` command to
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

| Channel | Mechanism |
| --- | --- |
| ServiceManager services | `BinderProxy.transactNative` JNI hook; enumeration and debug replies are filtered in the Parcel layer, direct app lookups are rewritten in a private request copy. Keywords: `lineage`, `crdroid`, `aospa`, `pixelexperience`, `omnirom`, `protonaosp`, plus the exact name `profile` |
| Resource package | AssetManager name/ID lookups treat `lineageos.platform` (resource package id `0x3f`) as absent |
| System features | `hasSystemFeature` requests are rewritten and the Parcel read path returns equal-length placeholders for the eight `org.lineageos.*` names |
| Protected broadcasts | the ten lineage protected actions are replaced in a private copy of outbound `IActivityManager` requests |
| Reflection constant | `AssetManager.LINEAGE_APK_PATH` is removed from `getDeclaredField`, `getField` and the `getDeclaredFields` family (AssetManager only) |

All in-process channels are always on, length-preserving, process-local and
never write into Binder reply buffers.

## Injection scope

There is no configuration.  The module injects a process when:

1. its uid is an application uid (`>= 10000`), and
2. its package is not a system package.

`service.sh` records the system packages at every boot
(`/data/adb/modules/lineage_hide/system_packages.txt`, generated from
`pm list packages -s`), so only third-party applications are touched.  In the
boot window before that list exists, a small hardcoded fallback protects
SystemUI, Settings, the permission controller, core providers, core services
and `org.lineageos.*` apps.  `FORCE_DENYLIST_UNMOUNT` is always requested.

To exclude an app, freeze it from the module by adding its package to
`system_packages.txt` (the file is regenerated on the next boot).

## Boot integration (`service.sh`)

After the boot animation starts, the module:

1. deletes system properties whose name contains `lineage` and rewrites values
   that contain `lineage`;
2. records the system package list used by the injection scope above;
3. walks `/system`, `/vendor`, `/system_ext` and `/product` for `*lineage*` and
   `*gapps*` entries and registers them with `ksu_susfs add_sus_path`
   (`add_sus_map` for regular files);
4. registers additional paths and maps: `addon.d`, the LineageOS update
   directory, SELinux policy files, the platform resource RRO and its idmaps,
   the module's own native library and the Zygisk library.

Every step is idempotent and safe to repeat on the next boot.

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

- In a third-party app: `getSystemAvailableFeatures()` shows no
  `org.lineageos.*` names, the `lineageos.platform` resource id `0x3f` is
  absent, and `AssetManager.class.getDeclaredField("LINEAGE_APK_PATH")` throws
  `NoSuchFieldException`.
- In a system app (Settings, SystemUI, `org.lineageos.*`) all of the above must
  still be visible, since those processes are never injected.
- `logcat -s LineageHide` shows the hook installation lines; `service.sh`
  output appears in the kernel/manager log.

## Boundaries

- Zygisk keeps the module `.so` mapped in zygote.  Updates require a reboot,
  and replacing the file under a live mapping can crash processes.
- `PendingIntent`-relayed broadcasts, native-only Binder lookups and native
  reflection are outside the current scope.
- Without SUSFS and the kernel patch, native scanners can still observe ROM
  files, mappings and properties from application processes.
