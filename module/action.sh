#!/system/bin/sh

MODULE_DIR="/data/adb/modules/lineage_hide"
CONFIG="$MODULE_DIR/config.json"
TEMP_CONFIG="$MODULE_DIR/config.json.tmp"
PACKAGE_LIST="$MODULE_DIR/targets.tmp"

if [ "$(id -u)" != "0" ]; then
    echo "Error: Root required"
    exit 1
fi

if [ ! -d "$MODULE_DIR" ]; then
    echo "Error: Lineage Hide module not found at $MODULE_DIR"
    exit 1
fi

# Preserve existing settings; defaults follow the shipped configuration.
ENABLED="true"
ENHANCED="true"
HIDE_LINEAGE_RESOURCES="true"
HIDE_LINEAGE_FEATURES="true"
HIDE_LINEAGE_BROADCASTS="true"
if [ -f "$CONFIG" ]; then
    grep -q '"enabled"[[:space:]]*:[[:space:]]*false' "$CONFIG" && ENABLED="false"
    grep -q '"force_denylist_unmount"[[:space:]]*:[[:space:]]*false' "$CONFIG" && ENHANCED="false"
    grep -q '"hide_lineage_resources"[[:space:]]*:[[:space:]]*false' "$CONFIG" && HIDE_LINEAGE_RESOURCES="false"
    grep -q '"hide_lineage_features"[[:space:]]*:[[:space:]]*false' "$CONFIG" && HIDE_LINEAGE_FEATURES="false"
    grep -q '"hide_lineage_broadcasts"[[:space:]]*:[[:space:]]*false' "$CONFIG" && HIDE_LINEAGE_BROADCASTS="false"
fi

CURRENT_USER="$(am get-current-user 2>/dev/null)"
[ -z "$CURRENT_USER" ] && CURRENT_USER="0"

pm list packages -3 --user "$CURRENT_USER" 2>/dev/null |
    sed -n 's/^package://p' |
    sort -u > "$PACKAGE_LIST"

PACKAGE_COUNT="$(wc -l < "$PACKAGE_LIST" | tr -d ' ')"

{
    echo "{"
    echo "  \"enabled\": $ENABLED,"
    echo "  \"force_denylist_unmount\": $ENHANCED,"
    echo "  \"hide_lineage_resources\": $HIDE_LINEAGE_RESOURCES,"
    echo "  \"hide_lineage_features\": $HIDE_LINEAGE_FEATURES,"
    echo "  \"hide_lineage_broadcasts\": $HIDE_LINEAGE_BROADCASTS,"
    echo "  \"targets\": ["

    INDEX=0
    while IFS= read -r PACKAGE_NAME; do
        [ -z "$PACKAGE_NAME" ] && continue
        INDEX=$((INDEX + 1))
        if [ "$INDEX" -lt "$PACKAGE_COUNT" ]; then
            printf '    "%s",\n' "$PACKAGE_NAME"
        else
            printf '    "%s"\n' "$PACKAGE_NAME"
        fi
    done < "$PACKAGE_LIST"

    echo "  ]"
    echo "}"
} > "$TEMP_CONFIG"

chmod 0644 "$TEMP_CONFIG"
mv -f "$TEMP_CONFIG" "$CONFIG"
rm -f "$PACKAGE_LIST"

echo "Lineage Hide config updated."
echo "  enabled: $ENABLED"
echo "  force_denylist_unmount: $ENHANCED"
echo "  hide_lineage_resources: $HIDE_LINEAGE_RESOURCES"
echo "  hide_lineage_features: $HIDE_LINEAGE_FEATURES"
echo "  hide_lineage_broadcasts: $HIDE_LINEAGE_BROADCASTS"
echo "  user: $CURRENT_USER"
echo "  targets: $PACKAGE_COUNT app(s)"
echo "  path: $CONFIG"
echo
echo "Force-stop target apps or reboot to apply."
