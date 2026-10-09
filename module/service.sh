#!/system/bin/sh

# Wait for the boot animation, bounded so a headless or unusual boot cannot
# block the service stage forever.
i=0
while [ "$(getprop init.svc.bootanim 2>/dev/null)" != "running" ] && [ "$i" -lt 120 ]; do
    sleep 1
    i=$((i + 1))
done

# Wait for primary storage (bounded).
i=0
while [ ! -d "/sdcard/Android" ] && [ "$i" -lt 180 ]; do
    sleep 1
    i=$((i + 1))
done

# Remove or rewrite system properties that carry the ROM name.
if command -v resetprop >/dev/null 2>&1; then
    resetprop | awk -F '\\[|\\]: \\[|\\]' '/lineage/ {
        key=$2
        value=$3
        if (key ~ /lineage/) {
            system("resetprop --delete \"" key "\"")
        } else if (value ~ /lineage/) {
            gsub("lineage_?", "", value)
            system("resetprop \"" key "\" \"" value "\"")
        }
    }'
fi

# Record the system packages so the Zygisk module can skip them without any
# user configuration: only third-party applications are injected.
MODDIR="${0%/*}"
pm list packages -s --user 0 2>/dev/null | sed 's/^package://' | sort -u > "$MODDIR/system_packages.txt.tmp"
if [ -s "$MODDIR/system_packages.txt.tmp" ]; then
    mv -f "$MODDIR/system_packages.txt.tmp" "$MODDIR/system_packages.txt"
else
    rm -f "$MODDIR/system_packages.txt.tmp"
fi

# Everything below registers SUSFS entries; without SUSFS the script stops
# here instead of logging a failure for every call.
if ! command -v ksu_susfs >/dev/null 2>&1; then
    echo "Lineage Hide: ksu_susfs not available (kernel without SUSFS?); skipping registration"
    exit 0
fi

find "/system" "/vendor" "/system_ext" "/product" -iname "*lineage*" -o -iname '*gapps*' | while IFS= read -r line
do
    ksu_susfs add_sus_path "$line"
    [ -f "$line" ] && ksu_susfs add_sus_map "$line"
done

ksu_susfs add_sus_path "/system/addon.d"
ksu_susfs add_sus_path "/data/lineageos_updates"
ksu_susfs add_sus_path "/system/lib64/libstagefright.so"
ksu_susfs add_sus_path "/data/adbroot"

ksu_susfs add_sus_path_loop "/vendor/etc/selinux/vendor_file_contexts"
ksu_susfs add_sus_path_loop "/system_ext/etc/selinux/system_ext_sepolicy.cil"
ksu_susfs add_sus_path_loop "/vendor/etc/selinux/vendor_sepolicy.cil"
ksu_susfs add_sus_map "/data/adb/modules/zygisksu/lib64/libzygisk.so"
ksu_susfs add_sus_map "/data/adb/modules/lineage_hide/zygisk/arm64-v8a.so"

ksu_susfs add_sus_map "/data/resource-cache/product@overlay@framework-res__lineage_kebab__auto_generated_rro_product.apk@idmap"
ksu_susfs add_sus_map "/data/resource-cache/vendor@overlay@org.lineageos.platform-res__lineage_kebab__auto_generated_rro_vendor.apk@idmap"
ksu_susfs add_sus_map "/product/overlay/framework-res__lineage_kebab__auto_generated_rro_product.apk"
ksu_susfs add_sus_map "/system/framework/org.lineageos.platform-res.apk"
ksu_susfs add_sus_map "/vendor/overlay/org.lineageos.platform-res__lineage_kebab__auto_generated_rro_vendor.apk"

sleep 3
ksu_susfs add_sus_path_loop "/vendor/etc/selinux/vendor_hwservice_contexts"
ksu_susfs add_sus_path_loop "/system_ext/etc/init/init.lineage-system_ext.rc"
ksu_susfs add_sus_path "/system_ext/etc/permissions"
