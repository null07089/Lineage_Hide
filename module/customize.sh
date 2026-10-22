#!/system/bin/sh
# Lineage Hide - installation script

ui_print "==============================="
ui_print " Lineage Hide Installer / 安装程序"
ui_print "==============================="

# Fix permissions for the boot integration and the bundled SUSFS helper.
set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm "$MODPATH/customize.sh" 0 0 0755
set_perm "$MODPATH/service.sh" 0 0 0755

ui_print "- Installation complete / 安装完成"
ui_print "- All third-party applications are covered automatically / 已自动覆盖全部三方应用"
ui_print "- Reboot the device to load the module / 请重启设备以加载模块"
