#!/system/bin/sh
# Lineage Hide - installation script

ui_print "==============================="
ui_print " Lineage Hide Installer"
ui_print "==============================="

# Fix permissions for the boot integration and the bundled SUSFS helper.
set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm "$MODPATH/customize.sh" 0 0 0755
set_perm "$MODPATH/service.sh" 0 0 0755

ui_print "- Installation complete"
ui_print "- All third-party applications are covered automatically"
ui_print "- Reboot the device to load the module"
