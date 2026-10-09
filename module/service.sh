#!/system/bin/sh
PATH="${0%/*}:$PATH"

resetprop -w init.svc.bootanim running

until [ -d "/sdcard/Android" ]
do
    sleep 1
done

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

ksu_susfs add_sus_path_loop "/vendor/etc/selinux/vendor_hwservice_contexts"
ksu_susfs add_sus_path_loop "/system_ext/etc/init/init.lineage-system_ext.rc"
ksu_susfs add_sus_path "/system_ext/etc/permissions"
