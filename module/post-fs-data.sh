#!/system/bin/sh

# Zygisk Next maps its own library and this module into zygote long before
# service.sh starts.  Register those paths with SUSFS at post-fs-data time so
# the mappings inherited by every application are already known to the kernel
# when zygote maps them.  service.sh repeats the registration for ROMs where
# the SUSFS command only becomes usable later.
SUSFS_BIN="/data/adb/ksu/bin/ksu_susfs"
if [ ! -x "$SUSFS_BIN" ]; then
    SUSFS_BIN="$(command -v ksu_susfs 2>/dev/null)"
fi
[ -n "$SUSFS_BIN" ] && [ -x "$SUSFS_BIN" ] || exit 0

for path in \
    "/data/adb/modules/zygisksu/lib64/libzygisk.so" \
    "/data/adb/modules/lineage_hide/zygisk/arm64-v8a.so"
do
    [ -e "$path" ] && "$SUSFS_BIN" add_sus_map "$path"
done

exit 0
